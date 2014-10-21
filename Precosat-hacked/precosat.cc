// Copyright (c) 2009, Armin Biere, JKU, Linz, Austria.  All rights reserved.

#include "precosat.hh"
#include "precobnr.hh"

#include <cstdio>
#include <cstring>
#include <cstdarg>

extern "C" {
#include <ctype.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/unistd.h>
#include <malloc/malloc.h>
};

#ifndef NLOGPRECO
#include <iostream>
#define LOG(code) \
  do { std::cout << "c LOG " << code << std::endl; } while (false)
#else
#define LOG(code) do { } while (false)
#endif

#if 0
#include <iostream>
#define COVER(code) \
  do { std::cout << "c COVER " << __FUNCTION__ << ' ' \
       << __LINE__ << ' ' << code << std::endl; } while (false)
#else
#define COVER(code) do { } while (false)
#endif

// #define PRECOCHECK // TODO check disabled!

#ifdef NSTATSPRECO
#define INC(s) do { stats.s++; } while (0)
#else
#define INC(s) do { } while (0)
#endif

namespace PrecoSat {

static void msg (const char * msg, ...);

inline static bool ispowoftwo (size_t n) { return !(n & (n-1)); }

inline static void * allocate (size_t bytes, size_t alignment = 0) {
  size_t mb = bytes >> 20;
  if (mb >= 100) msg ("allocating %d MB block", (int) mb);
  void * res;
  if (alignment && ispowoftwo (alignment)) res = memalign (alignment, bytes);
  else res = malloc (bytes);
  if (!res) die ("out of memory allocating %d MB", mb);
  return res;
}

inline static void * callocate (size_t bytes, size_t alignment = 0) {
  void * res = allocate (bytes, alignment);
  memset (res, 0, bytes);
  return res;
}

inline static void * reallocate (void * ptr, size_t bytes) {
  size_t mb = bytes >> 20;
  if (mb >= 100) msg ("reallocating %d MB block", (int) mb);
  void * res  = realloc (ptr, bytes);
  if (!res) die ("out of memory reallocating %d MB", (int) mb);
  return res;
}

template<class T> inline static void swap (T & a, T & b)
  { T tmp = a; a = b; b = tmp; }

template<class T> inline static const T min (const T a, const T b)
  { return a < b ? a : b; }

template<class T> inline static const T max (const T a, const T b)
  { return a > b ? a : b; }

template<class T> inline static void fix (T * & p, long moved) 
  { char * q = (char*) p; q += moved; p = (T*) q; }

inline static int logsize (int size) { return (128 >> size) + 1; }

static inline Val lit2val (int lit) { return (lit & 1) ? -1 : 1; }

static inline Cls * lit2conflict (Cls & bins, int a, int b) {
  bins.lits[0] = a;
  bins.lits[1] = b;
  assert (!bins.lits[2]);
  return &bins;
}

static bool hasgzsuffix (const char * str) 
  { return strlen (str) >= 3 && !strcmp (str + strlen (str) - 3, ".gz"); }

static inline unsigned ggt (unsigned a, unsigned b) 
  { while (b) { unsigned r = a % b; a = b, b = r; } return a; }

static inline double mb (size_t bytes) 
  { return bytes / (double) (1<<20); }

static inline bool sigsubs (unsigned s, unsigned t) 
  { return !(s&~t); }

static inline unsigned listig (int lit) 
  { return (1u << (31u & (unsigned)(lit/2))); }

static inline double average (double a, double b) { return b ? a/b : 0; }

static inline double percent (double a, double b) { return 100*average(a,b); }

static bool parity (unsigned x)
  { bool res = false; while (x) res = !res, x &= x-1; return res; }
}

using namespace PrecoSat;

inline unsigned RNG::next () {
  unsigned res = state;
  state *= 1664525u;
  state += 1013904223u;
  return res;
}

inline bool RNG::oneoutof (unsigned spread) {
  return spread ? !(next () % spread) : true;
}

inline size_t Cls::bytes (int n) { 
  return sizeof (Cls) + (n - 3 + 1) * sizeof (int); 
}

inline size_t Cls::bytes () const { return bytes (size); }

static Cls hash_removed_cls;
Cls * Hash::REMOVED = &hash_removed_cls;

inline bool Hash::match (Cls * cls, int a, int b , int c) {
  if (cls == REMOVED) return false;
  int m = cls->lits[0], n = cls->lits[1], o = cls->lits[2];
  if (m == a) {
    if (n == b) return o == c;
    if (n == c) return o == b;
  } else if (m == b) {
    if (n == a) return o == c;
    if (n == c) return o == a;
  } else if (m == c) {
    if (n == a) return o == b;
    if (n == b) return o == a;
  }
  return false;
}

inline unsigned Hash::hash (int a, int b, int c) {
  assert (a > b && b > c);
  unsigned res = 1000000007u * (unsigned)a;
  res +=         2000000011u * (unsigned)b;
  res +=         3000000019u * (unsigned)c;
  assert (size >= 1);
  res &= (size - 1);
  assert (res < size);
  return res;
}

inline unsigned Hash::delta (int a, int b, int c) {
  assert (a > b && b > c);
  unsigned res = 4000000007u * (unsigned)a;
  res +=         3000000019u * (unsigned)b;
  res +=         2000000011u * (unsigned)c;
  assert (size >= 2);
  res &= (size - 1);
  if (!(res & 1)) res++;
  assert (res < size);
  return res;
}

inline Cls ** Hash::find (int a, int b, int c, bool overwrite) {
#ifndef NSTATSPRECO
  hashed++;
#endif
  assert (size > 0);
  if (a < b) swap (a, b);
  if (a < c) swap (a, c);
  if (b < c) swap (b, c);
  unsigned h = hash (a, b, c);
  Cls ** res = table + h, * cls = *res;
  if (overwrite && cls == REMOVED) return res;
  if (!cls || match (cls, a, b, c)) return res;
#ifndef NSTATSPRECO
  collisions++;
#endif
  unsigned d = delta (a, b, c);
  for (;;) {
    h += d;
    if (h >= size) h -= size;
    assert (h < size);
    res = table + h, cls = *res;
    if (overwrite && cls == REMOVED) return res;
    if (!cls || match (cls, a, b, c)) return res;
#ifndef NSTATSPRECO
    collisions++;
#endif
  }
}

inline void Hash::enlarge (Mem & mem) {
  unsigned oldsize = size;
  Cls ** oldtable = table;
  if (removed > size/4) size = oldsize;
  else size = oldsize ? 2 * oldsize : 4;
  size_t bytes = size * sizeof *table;
  table = (Cls**) callocate (bytes);
  for (Cls ** p = oldtable; p < oldtable + oldsize; p++) {
    Cls * cls = *p;
    if (!cls || cls == REMOVED) continue;
    Cls ** q = find (cls->lits[0], cls->lits[1], cls->lits[2]);
    assert (!*q);
    *q = cls;
  }
  mem += bytes;
  bytes = oldsize  * sizeof *oldtable;
  mem -= bytes;
  free (oldtable);
  assert (removed <= count);
  count -= removed;
  removed = 0;
}

inline Cls * Hash::contains (int a, int b, int c) {
  if (!size) return 0;
  Cls * res = *find (a, b, c);
  return res == REMOVED ? 0 : res;
}

inline bool Hash::cntnd (int a, int b, int c) {
  Cls * cls = contains (a, b, c);
  if (!cls) return false;
  if (cls->dirty) return false;
  //if (cls->garbage) return false;//TODO really?
  return true;
}

inline void Hash::insert (Mem & mem, Cls * cls) {
  assert (!cls->hashed);
  assert (cls->lits[0] && cls->lits[1]);
  assert (!cls->lits[2] || !cls->lits[3]);
  if (2 * count >= size) enlarge (mem);
  Cls ** p = find (cls->lits[0],cls->lits[1],cls->lits[2],0);
  if (*p && *p != REMOVED) return;
  if (*p == REMOVED) { 
    assert (removed > 0), removed--;
    assert (count > 0), count--;
  }
  *p = cls;
  cls->hashed = true;
  count++;
#ifndef NDDEBUG
  if (!cls->lits[2]) assert (contains (cls->lits[0], cls->lits[1]));
  else if (!cls->lits[3]) 
    assert (contains (cls->lits[0], cls->lits[1], cls->lits[2]));
#endif
}

inline void Hash::remove (Cls * cls) {
  assert (cls->hashed);
  Cls ** p = find (cls->lits[0], cls->lits[1], cls->lits[2]);
  assert (*p == cls);
  cls->hashed = false;
  *p = REMOVED;
  removed++;
}

inline void Hash::release (Mem & mem) {
  for (Cls ** p = table; p < table + size; p++) {
    Cls * c = *p;
    if (!c || c == REMOVED) continue;
    assert (c->hashed);
    c->hashed = false;
  }
  size_t bytes = size * sizeof *table;
  mem -= bytes;
  free (table);
  table = 0;
  size = count = removed = 0;
}

inline Anchor<Cls> & Solver::anchor (Cls * c) {
  if (c->binary) return binary;
  return c->lnd ? learned : original;
}

inline int Cls::minlit () const {
  int res = INT_MAX, other;
  for (const int * p = lits; (other = *p); p++)
    if (other < res) res = other;
  assert (res  < INT_MAX);
  return res;
}

inline bool Cls::contains (int lit) const {
  if (!(sig & listig (lit))) return false;
  for (const int * p = lits; *p; p++)
    if (*p == lit) return true;
  return false;
}

inline void Solver::setsig (Cls * cls) {
  int except = cls->minlit (), lit;
  unsigned fwsig = 0, bwsig = 0;
  for (const int * p = cls->lits; (lit = *p); p++) {
    unsigned sig = listig (lit);
    bwsig |= sig;
    if (lit != except) fwsig |= sig;
  }
  cls->sig = bwsig;
  if (bwsigs)
    for (const int * p = cls->lits; (lit = *p); p++)
      bwsigs[lit] |= bwsig;
  if (fwsigs)
    fwsigs[except] |= fwsig;
}

inline unsigned Solver::litsig () {
  unsigned res = 0;
  for (int i = 0; i < lits; i++) 
    res |= listig (lits[i]);
  return res;
}

inline void Solver::connect (Cls * c) {
  assert (c->lits[0] && c->lits[1]);
  Anchor<Cls> & a = anchor (c);
  if (!connected (a, c)) push (a, c);
  for (int i = 0; i <= 1; i++)
    occs[c->lits[i]].push (mem, Occ (c->lits[!i], c->binary ? 0 : c));
  if (opts.hash && !c->lits[2]) hash.insert (mem, c);
  if (!orgs || c->lnd) return;
  for (const int * p = c->lits; *p; p++)
    orgs[*p].push (mem, c);
  if (fwds) fwds[c->minlit ()].push (mem, c);
  setsig (c);
}

inline void Solver::disconnect (Cls * c) {
  assert (!c->locked && !c->dirty && !c->trash);
  int l0 = c->lits[0], l1 = c->lits[1];
  assert (l0 && l1);
  Anchor<Cls> & a = anchor (c);
  if (connected (a, c)) dequeue (a, c);
  if (c->binary) occs[l0].remove(Occ(l1,0)), occs[l1].remove(Occ(l0,0));
  else occs[l0].remove(Occ(0,c)), occs[l1].remove(Occ(0,c));
  if (c->hashed) hash.remove (c);
  if (!orgs || c->lnd) return;
  for (const int * p = c->lits; *p; p++)
    orgs[*p].remove (c);
  if (fwds) fwds[c->minlit ()].remove (c);
}

inline Rnk * Solver::prb (const Rnk * r) { return prbs + (r - rnks); }
inline Rnk * Solver::rnk (const Var * v) { return rnks + (v - vars); }
inline Var * Solver::var (const Rnk * r) { return vars + (r - rnks); }

inline Val Solver::fixed (int lit) const {
  return vars[lit/2].dlevel ? 0 : vals[lit];
}

inline void Solver::collect (Cls * cls) {
  assert (!cls->locked && !cls->dirty && !cls->trash && !cls->gate);
  if (cls->binary) assert (stats.clauses.binary), stats.clauses.binary--;
  else if (cls->lnd) assert (stats.clauses.learned), stats.clauses.learned--;
  else assert (stats.clauses.original), stats.clauses.original--;
  size_t bytes = cls->bytes ();
  assert (mem >= bytes);
  mem -= bytes;
#ifdef PRECOCHECK
  if (!cls->lnd) {
    for (const int * p = cls->lits; *p; p++)
      check.push (mem, *p);
    check.push (mem, 0);
  }
#endif
  free (cls);
  stats.collected += bytes;
}

void Solver::touch (int lit) {
  if (vals[lit]) return;
  if (repr[lit]) return;
  int idx = lit/2;
  Var * v = vars + idx;
  if (v->eliminated) return;
  assert (orgs);
  Rnk * e = elms + idx;
  int pos = orgs[lit];
  int neg = orgs[1^lit];
  int nh = INT_MIN; 
  if (pos <= (1<<9) || neg <= (1<<9)) {
    if ((pos < (1<<22) && neg < (1<<22)) || pos <= 1 || neg <= 1) {
      int sum = pos + neg, prod = pos * neg;
      assert (0 <= sum && 0 <= prod && prod <= (int)((1u<<31)-1));
      nh = sum - prod;
    }
  }
  int oh = e->heat; e->heat = nh;
  if (oh != nh) LOG ("touch " << lit << " from " << oh << " to " << nh);
  if (!schedule.elim.contains (e)) schedule.elim.push (mem, e);
  else if (nh > oh) schedule.elim.up (e);
  else schedule.elim.down (e);
}

void Solver::touch (Cls * c) {
  assert (!c->lnd);
  assert (elimode);
  for (int * p = c->lits; *p; p++)
    touch (*p);
}

inline void Solver::recycle (Cls * c) {
  disconnect (c);
  if (c->trash) trash.remove (c);
  if (c->gate) gate.remove (c);
  if (elimode) touch (c);
  collect (c);
}

inline void Solver::dump (Cls * c) {
  assert (!c->dirty);
  if (c->trash) return;
  c->trash = true;
  trash.push (mem, c);
}

inline void Solver::cleantrash () {
  if (!trash) return;
  int old = stats.clauses.original;
  while (trash) {
    Cls * cls = trash.pop ();
    assert (cls->trash);
    cls->trash = false;
    recycle (cls);
  }
  shrink (old);
}

inline void Solver::gcls (Cls * c) {
  assert (!c->gate);
  c->gate = true;
  gate.push (mem, c);
}

inline void Solver::cleangate () {
  while (gate) {
    Cls * cls = gate.pop ();
    assert (cls->gate); cls->gate = false;
  }
  gatepivot = 0;
  gatestats = 0;
  gatelen = 0;
}

inline void Solver::recycle (int lit) {
  LOG ("recycled " << lit);
  assert (!level);
  assert (fixed (lit));
  assert (vals[lit] > 0);
  if (!orgs) return;
  while (orgs[lit]) recycle (orgs[lit][0]);
}

Opt::Opt (const char * n, int v, int * vp, int mi, int ma) :
name (n), valptr (vp), min (mi), max (ma) 
{ 
    assert (min <= v);
    assert (v <= max);
    *vp = v; 
}

bool Opts::set (const char * opt, int val) {
  for (Opt * o = opts.begin (); o < opts.end (); o++)
    if (!strcmp (o->name, opt)) { 
      if (val < o->min) val = o->min;
      if (val > o->max) val = o->max;
      *o->valptr = val; 
      return true;
    }
  return false;
}

void Opts::add (Mem & mem, const char * n, int v, int * vp, int mi, int ma) {
  opts.push (mem, Opt (n, v, vp, mi, ma));
}

void Opts::print () const {
  const Opt * o = opts.begin ();
  fputc ('c', stdout); int pos = 1;
  while (o < opts.end ()) {
    char line[80];
    sprintf (line, " --%s=%d", o->name, *o->valptr);
    int len = strlen (line); assert (len < 80);
    if (len + pos >= 76) {
      fputs ("\nc", stdout);
      pos = 1;
    }
    fputs (line, stdout);
    pos += len;
    o++;
  }
  fputc ('\n', stdout);
}

void Solver::initorgs () {
  size_t bytes = 2 * (size + 1) * sizeof *orgs;
  orgs = (Orgs*)  callocate (bytes, sizeof *orgs);
  mem += bytes;
}

void Solver::delorgs () {
  for (int lit = 2; lit <= 2*size+1; lit++) orgs[lit].release (mem);
  size_t bytes = 2 * (size + 1) * sizeof *orgs;
  mem -= bytes;
  free (orgs);
  orgs = 0;
}

void Solver::initfwds () {
  size_t bytes = 2 * (size + 1) * sizeof *fwds;
  fwds = (Fwds*)  callocate (bytes, sizeof *fwds);
  mem += bytes;
}

void Solver::delfwds () {
  for (int lit = 2; lit <= 2*size+1; lit++) fwds[lit].release (mem);
  size_t bytes = 2 * (size + 1) * sizeof *fwds;
  mem -= bytes;
  free (fwds);
  fwds = 0;
}

void Solver::initsigs () {
  size_t bytes = 2 * (size + 1) * sizeof *bwsigs;
  bwsigs = (unsigned*)  callocate (bytes, sizeof *bwsigs);
  fwsigs = (unsigned*)  callocate (bytes, sizeof *fwsigs);
  mem += 2 * bytes;
}

void Solver::delsigs () {
  size_t bytes = 2 * (size + 1) * sizeof *bwsigs;
  mem -= 2 * bytes;
  free (bwsigs), free (fwsigs);
  bwsigs = fwsigs = 0;
}

#define OPT(n,v,mi,ma) \
do { opts.add (mem, # n, v, &opts.n, mi, ma); } while (0)

Solver::Solver (int s) :
  size(s), queue(0), queue2 (0), level (0), conflict(0), 
  hinc (128), agility (0), 
  measure (true), iterating (false), elimode (false), extending (false)
{
  size_t bytes = sizeof *vars; bytes *= size + 1;
  vars = (Var*) callocate (bytes, sizeof *vars); mem += bytes;

  iirfs = 0;

  bytes = sizeof *repr; bytes *= size + 1; bytes *= 2;
  repr = (int *) callocate (bytes); mem += bytes;

  bytes = sizeof *jwhs; bytes *= size + 1; bytes *= 2;
  jwhs = (int*) callocate (bytes); mem += bytes;

  bytes = sizeof *rnks; bytes *= size + 1;
  rnks = (Rnk*) allocate (bytes); mem += bytes;
  for (Rnk * p = rnks + size; p > rnks; p--)
    p->heat = 0, p->pos = -1, schedule.decide.push (mem, p);

  bytes = sizeof *prbs; bytes *= size + 1;
  prbs = (Rnk*) allocate (bytes); mem += bytes;
  for (Rnk * p = prbs + size; p > prbs; p--)
    p->heat = 0, p->pos = -1;

  bytes = sizeof *elms; bytes *= size + 1;
  elms = (Rnk*) allocate (bytes); mem += bytes;
  for (Rnk * p = elms + size; p > elms; p--)
    p->heat = 0, p->pos = -1;

  bytes = sizeof *vals; bytes *= size + 1; bytes *= 2;
  vals = (Val*) callocate (bytes); mem += bytes;

  bytes = sizeof *occs; bytes *= size + 1; bytes *= 2;
  occs = (Occs*) callocate (bytes, sizeof *occs); mem += bytes;

  orgs = 0;
  fwds = 0;
  fwsigs = 0;
  bwsigs = 0;

  frames.push (mem, Frame (trail));

  empty.lits[0] = 0;
  dummy.lits[2] = 0;

  int M = INT_MAX;
  OPT (check,0,0,2);
  OPT (verbose,0,0,1);
  OPT (order,3,1,9);
  OPT (simprd,40,0,M);
  OPT (merge,1,0,1);
  OPT (hash,1,0,1);
  OPT (dominate,1,0,1);
  OPT (restart,1,0,1); OPT (restartint,100,1,M);
  OPT (rebias,1,0,1); OPT (rebiasint,1000,1,M);
  OPT (probe,1,0,1); OPT (probeint,100*1000,1000,M); OPT (probeprd,10,1,M);
  OPT (decompose,1,0,1);
  OPT (minimize,3,0,3); OPT (maxdepth,1000,2,10000); OPT (strength,10,0,M);
  OPT (elim,1,0,1); OPT (elimint,300*1000,0,M); OPT (elimprd,20,1,M);
  OPT (subst,1,0,1); OPT (ands,1,0,1); OPT (xors,1,0,1); OPT (ites,1,0,1);
  OPT (minlimit,1000,100,10000); OPT (maxlimit,10*1000*1000,10000,M);
  OPT (random,1,0,1); OPT (spread,2000,0,M); OPT (seed,0,0,M);
  OPT (skip,25,0,100);
}

void Solver::deliirfs () {
  assert (iirfs);
  assert (opts.order >= 2);
  size_t bytes = (opts.order - 1) * (size + 1) * sizeof *iirfs;
  mem -= bytes;
  free (iirfs);
  iirfs = 0;
}

void Solver::delclauses (Anchor<Cls> & anchor) {
  Cls * prev;
  for (Cls * p = anchor.head; p; p = prev) {
    mem -= p->bytes (); prev = p->prev; free (p);
  }
  anchor.head = anchor.tail  = 0;
}

Solver::~Solver () {
#ifndef NDEBUG
  size_t bytes;
  for (int lit = 2; lit <= 2 * size + 1; lit++) occs[lit].release (mem);
  bytes = 2 * (size + 1) * sizeof *occs; mem -= bytes; free (occs);
  bytes = 2 * (size + 1) * sizeof *vals; mem -= bytes; free (vals);
  bytes = 2 * (size + 1) * sizeof *repr; mem -= bytes; free (repr);
  bytes = 2 * (size + 1) * sizeof *jwhs; mem -= bytes; free (jwhs);
  bytes = (size + 1) * sizeof *vars; mem -= bytes; free (vars);
  bytes = (size + 1) * sizeof *prbs; mem -= bytes; free (prbs);
  bytes = (size + 1) * sizeof *rnks; mem -= bytes; free (rnks);
  bytes = (size + 1) * sizeof *elms; mem -= bytes; free (elms);
  hash.release (mem);
  delclauses (original);
  delclauses (binary);
  delclauses (learned);
  if (iirfs) deliirfs ();
  if (orgs) delorgs ();
  if (fwds) delfwds ();
  if (fwsigs) delsigs ();
  schedule.elim.release (mem);
  schedule.decide.release (mem);
  schedule.probe.release (mem);
  opts.opts.release (mem);
  trail.release (mem);
  frames.release (mem);
  levels.release (mem);
  pulled.release (mem);
  trash.release (mem);
  gate.release (mem);
  saved.release (mem);
  elits.release (mem);
#ifdef PRECOCHECK
  check.release (mem);
#endif
  units.release (mem);
  lits.release (mem);
  seen.release (mem);
  assert (!mem);
#endif
}

void Solver::fxopts () { 
  assert (!opts.fixed);
  if (opts.order >= 2) {
    size_t bytes = (size + 1) * (opts.order - 1) * sizeof *iirfs;
    size_t alignment = (opts.order - 1) * sizeof *iirfs;
    iirfs =  (int *) allocate (bytes, alignment);
    mem += bytes;
    for (Var * v = vars + 1; v <= vars + size; v++)
      for (int i = 1; i < opts.order; i++)
	iirf (v, i) = -1;
  }
  opts.fixed = true;
}

#define PRSIZE(a) \
  do { \
  printf ("c sizeof %8s[0] = %2ld   alignment = %-2ld %6.0f MB\n", \
          # a, \
	  (long) sizeof a[0],  \
	  ((long) a) % sizeof a[0], \
	  mb ((size + 1) * sizeof a[0])); \
  } while (0)

void Solver::propts () { 
  assert (opts.fixed);
  opts.print ();
  printf ("c\n");
  PRSIZE (vars);
  PRSIZE (orgs);
  PRSIZE (occs);
  if (opts.order >= 2) {
    size_t alignment = (opts.order - 1) * sizeof *iirfs;
    printf ("c sizeof iirfs[0..%d] = %2ld   alignment = %-2ld %6.0f MB\n",
       	    opts.order-2, (long) alignment, 
	    ((long) iirfs) % alignment,
	    mb ((opts.order - 1) * (size + 1) * sizeof *iirfs));
  }
  fflush (stdout);
}

void Solver::prstats () {
  double t = stats.seconds ();
  printf ("c %d conflicts, %d decisions, %d random\n",
          stats.conflicts, stats.decisions, stats.random);
  printf ("c %d iterations, %d restarts, %d skipped\n", 
          stats.iter, stats.restart.count, stats.restart.skipped);
  printf ("c %d enlarged, %d shrunken, %d rescored, %d rebiased\n", 
          stats.enlarged, stats.shrunken, stats.rescored, stats.rebias.count);
  printf ("c %d simplifications, %d reductions\n",
          stats.simps, stats.reductions);
  printf ("c\n");
  printf ("c vars: %d fixed, %d equiv, %d elim, %d merged\n",
          stats.vars.fixed, stats.vars.equiv, 
	  stats.vars.elim, stats.vars.merged);
  printf ("c elim: %lld resolutions, %d phases, %d rounds\n",
          stats.elim.resolutions, stats.elim.phases, stats.elim.rounds);
  printf ("c sbst: %.0f%% substituted, "
          "%.1f%% nots, %.1f%% ands, %.1f%% xors, %.1f%% ites\n",
          percent (stats.vars.subst,stats.vars.elim),
	  percent (stats.subst.nots.count,stats.vars.subst),
	  percent (stats.subst.ands.count,stats.vars.subst),
	  percent (stats.subst.xors.count,stats.vars.subst),
	  percent (stats.subst.ites.count,stats.vars.subst));
  printf ("c arty: %.2f and %.2f xor average arity\n",
          average (stats.subst.ands.len, stats.subst.ands.count),
          average (stats.subst.xors.len, stats.subst.xors.count));
  printf ("c prbe: %d probed, %d phases, %d rounds\n",
	  stats.probe.variables, stats.probe.phases, stats.probe.rounds);
  printf ("c prbe: %d failed, %d lifted, %d merged\n",
          stats.probe.failed, stats.probe.lifted, stats.probe.merged);
  printf ("c sccs: %d non trivial, %d fixed, %d merged\n",
          stats.sccs.nontriv, stats.sccs.fixed, stats.sccs.merged);
  printf ("c hash: "
#ifndef NSTATSPRECO
          "%lld searches, %.0f%% collisions, "
#endif
	  "%d units, %d merged\n",
#ifndef NSTATSPRECO
          hash.hashed, percent (hash.collisions, hash.hashed),
#endif
          stats.hash.units, stats.hash.merged);
#ifndef NSTATSPRECO
  long long l1s = stats.sigs.bw.l1.srch + stats.sigs.fw.l1.srch;
  long long l1h = stats.sigs.bw.l1.hits + stats.sigs.fw.l1.hits;
  long long l2s = stats.sigs.bw.l2.srch + stats.sigs.fw.l2.srch;
  long long l2h = stats.sigs.bw.l2.hits + stats.sigs.fw.l2.hits;
  long long bws = stats.sigs.bw.l1.srch + stats.sigs.bw.l2.srch;
  long long bwh = stats.sigs.bw.l1.hits + stats.sigs.bw.l2.hits;
  long long fws = stats.sigs.fw.l1.srch + stats.sigs.fw.l2.srch;
  long long fwh = stats.sigs.fw.l1.hits + stats.sigs.fw.l2.hits;
  long long hits = bwh + fwh, srch = bws + fws;
  if (opts.verbose) {
    printf ("c sigs: %13lld srch %3.0f%% hits, %3.0f%% L1, %3.0f%% L2\n",
	     srch, percent (hits,srch), percent (l1h,l1s), percent(l2h,l2s));
    printf ("c   fw: %13lld %3.0f%% %3.0f%% hits, %3.0f%% L1, %3.0f%% L2\n",
	    fws, percent (fws,srch), percent (fwh,fws),
	    percent (stats.sigs.fw.l1.hits, stats.sigs.fw.l1.srch),
	    percent (stats.sigs.fw.l2.hits, stats.sigs.fw.l2.srch));
    printf ("c   bw: %13lld %3.0f%% %3.0f%% hits, %3.0f%% L1, %3.0f%% L2\n",
	    bws, percent (bws,srch), percent (bwh,bws),
	    percent (stats.sigs.bw.l1.hits, stats.sigs.bw.l1.srch),
	    percent (stats.sigs.bw.l2.hits, stats.sigs.bw.l2.srch));
  } else
    printf ("c sigs: %lld searched, %.0f%% hits, %.0f%% L1, %.0f%% L2\n",
	     srch, percent (hits,srch), percent (l1h,l1s), percent(l2h,l2s));
#endif
  long long alllits = stats.lits.added + stats.mins.deleted;
  printf ("c mins: %lld learned, %.0f%% deleted, %lld strong, %d depth\n",
          stats.lits.added,
	  percent (stats.mins.deleted, alllits),
	  stats.mins.strong, stats.mins.depth);
  printf ("c subs: %d forward, %d backward, %d doms\n",
	  stats.subs.fw, stats.subs.bw, stats.subs.doms);
  printf ("c strs: %d forward, %d backward\n",
	  stats.str.fw, stats.str.bw);
  assert (stats.doms.count >= stats.doms.level1);
  assert (stats.doms.level1 >= stats.doms.probing);
  printf ("c doms: %d dominators, %d high, %d low\n", 
          stats.doms.count, 
	  stats.doms.count - stats.doms.level1,
	  stats.doms.level1 - stats.doms.probing);
#ifndef NSTATSPRECO
  printf ("c vsts: %lld visits, %.2f visits/propagation, %.0f%% ternary\n",
          stats.visits, average (stats.visits, stats.props),
	  percent (stats.ternaryvisits, stats.visits));
#endif
  printf ("c prps: %lld propagations, %.2f megaprops\n",
          stats.props, t ? stats.props/1e6/t : 0);
  printf ("c\n");
  printf ("c %.1f seconds, %.0f MB max, %.0f MB recycled\n", 
         t, mb (mem.max), mb (stats.collected));
  fflush (stdout);
}

inline void Solver::assign (int lit) {
  assert (!vals [lit]);
  vals[lit] = 1; vals[lit^1] = -1;
  Var & v = vars[lit/2];
  assert (v.eliminated == extending);
  if (!(v.dlevel = level)) {
    if (repr[lit]) {
      assert (stats.vars.equiv), stats.vars.equiv--;
    }
    stats.vars.fixed++;
  }
  if (measure) {
    Val val = lit2val (lit);
    agility -= agility/10000;
    if (v.phase && v.phase != val) agility += 1000;
    v.phase = val;
  }
  v.tlevel = trail;
  trail.push (mem, lit);
#ifndef NLOGPRECO
  printf ("c LOG assign %d at level %d <=", lit, level);
  if (v.binary) printf (" %d %d\n", lit, v.reason.lit);
  else if (v.reason.cls)
    printf (" %d %d %d%s\n",
	    v.reason.cls->lits[0],
	    v.reason.cls->lits[1],
	    v.reason.cls->lits[2],
	    v.reason.cls->size > 3 ? " ..." : "");
  else if (!level) printf (" top level\n");
  else printf (" decision\n");
#endif
}

inline void Solver::assume (int lit) {
  if (level >= Var::MAXL) die ("maximum decision level exceeded");
  frames.push (mem, Frame (trail)); level++; assert (level + 1 == frames);
  Var & v = vars[lit/2]; v.binary = false; v.reason.cls = 0;
  LOG ("assume " << lit);
  v.dominator = lit;
  assign (lit);
}

inline void Solver::imply (int lit, int reason) {
  assert (vals[reason] < 0);
  Var & v = vars[lit/2];
  if (level) v.binary = true, v.reason.lit = reason;
  else v.binary = false, v.reason.lit = 0;
  if (level) v.dominator = vars[reason/2].dominator;
  assign (lit);
}

inline int Solver::dominator (int lit, Cls * reason, bool & contained) {
  contained = false;
  if (!opts.dominate) return 0;
  assert (level > 0);
  int vdom = 0, other, oldvdom;
  Var * u;
  for (const int * p = reason->lits; vdom >= 0 && (other = *p); p++) {
    if (other == lit) continue;
    u = vars + (other/2);
    if (!u->dlevel) continue;
    if (u->dlevel < level) { vdom = -1; break; }
    int udom = u->dominator;
    assert (udom);
    if (vdom) {
      assert (vdom > 0);
      if (udom != vdom) vdom = -1;
    } else vdom = udom;
  }
  assert (vdom);
  if (vdom <= 0) return vdom;
  assert (vals[vdom] > 0);
  LOG (vdom << " dominates " << lit);
  for (const int * p = reason->lits; !contained && (other = *p); p++)
    contained = (other^1) == vdom;
  if (contained) goto DONE;
  oldvdom = vdom; vdom = 0;
  for (const int * p = reason->lits; (other = *p); p++) {
    if (other == lit) continue;
    assert (vals[other] < 0);
    u = vars + other/2;
    if (!u->dlevel) continue;
    assert (u->dlevel == level);
    assert (u->dominator == oldvdom);
    other ^= 1;
    assert (other != oldvdom);
    if (other == vdom) continue;
    if (vdom) {
      while (!u->mark && 
	     (other = (assert (u->binary), 1^u->reason.lit)) != oldvdom && 
	     other != vdom) {
	assert (vals[other] > 0);
	u = vars + other/2;
	assert (u->dlevel == level);
	assert (u->dominator == oldvdom);
      }
      while (vdom != other) {
	u = vars + (vdom/2);
	assert (u->mark);
	u->mark = 0;
	assert (u->binary);
	vdom = 1^u->reason.lit;
      }
      if (vdom == oldvdom) break;
    } else {
      vdom = 1^u->reason.lit;
      if (vdom == oldvdom) break;
      assert (vals[vdom] > 0);
      other = vdom;
      do {
	u = vars + other/2;
	assert (u->dlevel == level);
	assert (u->dominator == oldvdom);
	assert (!u->mark);
	u->mark = 1;
	assert (u->binary);
	other = 1^u->reason.lit;
	assert (vals[other] > 0);
      } while (other != oldvdom);
    }
  }
  other = vdom;
  while (other != oldvdom) {
    u = vars + other/2;
    assert (u->dlevel == level);
    assert (u->dominator == oldvdom);
    assert (u->mark);
    u->mark = 0;
    assert (u->binary);
    other = 1^u->reason.lit;
    assert (vals[other] > 0);
  }

  if (vdom == oldvdom) return vdom;

  assert (vdom);
  LOG (vdom << " also dominates " << lit);
  assert (!contained);
  for (const int * p = reason->lits; !contained && (other = *p); p++)
    contained = (other^1) == vdom;

DONE:
  stats.doms.count++;
  if (level == 1) stats.doms.level1++;
  if (!measure) { assert (level == 1); stats.doms.probing++; }
  if (contained) {
    reason->garbage = true;
    stats.subs.doms++;
    LOG ("dominator clause is subsuming");
  }

  return vdom;
}

inline void Solver::unit (int lit) {
  Var * v;
  Val val = vals[lit];
  assert (!level);
  if (val < 0) { 
    LOG ("conflict after adding unit"); conflict = &empty; return;
  }
  if (!val) {
    v = vars + (lit/2);
    v->binary = false, v->reason.cls = 0;
    assign (lit);
  }
  int other = find (lit);
  if (other == lit) return;
  val = vals[other];
  if (val < 0) {
    LOG ("conflict after adding unit"); conflict = &empty; return;
  }
  if (val) return;
  v = vars + (other/2);
  v->binary = false, v->reason.cls = 0;
  assign (other);
}

inline void Solver::force (int lit, Cls * reason) {
  assert (reason);
  assert (!reason->binary);
  Val val = vals[lit];
  if (val < 0) { LOG ("conflict forcing literal"); conflict = reason; }
  if (val) return;
#ifndef NDEBUG
  for (const int * p = reason->lits; *p; p++)
    if (*p != lit) assert (vals[*p] < 0);
#endif
  Var * v = vars + (lit/2);
  int vdom;
  bool sub;
  if (!level) {
    v->binary = false, v->reason.cls = 0;
  } else if ((vdom = dominator (lit, reason, sub)) > 0)  {
    v->dominator = vdom;
    assert (vals[vdom] > 0);
    vdom ^= 1;
    LOG ("learned clause " << vdom << ' ' << lit);
    assert (!lits);
    lits.push (mem, vdom);
    lits.push (mem, lit);
    clause (reason->lnd || !sub);
    v->binary = true, v->reason.lit = vdom;
  } else {
    v->binary = false, v->reason.cls = reason;
    reason->locked = true;
    if (reason->lnd) stats.clauses.locked++;
    v->dominator = lit;
  }
  assign (lit);
}

inline void Solver::jwh (Cls * cls) {
  //if (cls->lnd) return; // TODO better not ?
  int * p;
  for (p = cls->lits; *p; p++)
    ;
  int size = p - cls->lits;
  int inc = logsize (size);
  while (p > cls->lits) {
    int l = *--p;
    jwhs[l] += inc; 
    if (jwhs[l] < 0) die ("maximum large JWH score exceeded");
  }
}

inline int Solver::find (int a) {
  assert (2 <= a && a <= 2 * size + 1);
  int res, tmp;
  for (res = a; (tmp = repr[res]); res = tmp)
    ;
  for (int fix = a; (tmp = repr[fix]) && tmp != res; fix = tmp)
    repr[fix] = res, repr[fix^1] = res^1;
  return res;
}

inline void Solver::merge (int l, int k, int & merged) {
  int a, b;
  if (!opts.merge) return;
  if (elimode) return;
  if ((a = find (l)) == (b = find (k))) return;
  assert (a != b);
  assert (a != (b^1));
#ifndef NLOGPRECO
  int m = min (a, b);
  LOG ("merge " << l << " and " << k << " to " << m);
  if (k != m) LOG ("learned clause " << k << ' ' << (m^1));
  if (k != m) LOG ("learned clause " << (k^1) << ' ' << m);
  if (l != m) LOG ("learned clause " << l << ' ' << (m^1));
  if (l != m) LOG ("learned clause " << (l^1) << ' ' << m);
#endif
  if (a < b) repr[k] = repr[b] = a, repr[k^1] = repr[b^1] = a^1;
  else       repr[l] = repr[a] = b, repr[l^1] = repr[a^1] = b^1;
  stats.vars.equiv++;
  stats.vars.merged++;
  merged++;
}

Cls * Solver::clause (bool lnd) {
  assert (!elimode || !lnd);
  Cls * res = 0;
#ifndef NLOGPRECO
  std::cout << "c LOG " << (lnd ? "learned" : "original") << " clause";
  for (const int * p = lits.begin (); p < lits.end (); p++)
    std::cout << ' ' << *p;
  std::cout << std::endl;
#endif
#ifndef NDEBUG
  for (int i = 0; i < lits; i++)
    assert (!vars[lits[i]/2].eliminated);
#endif
  if (lits == 0) {
    LOG ("conflict after added empty clause");
    conflict = &empty;
  } else if (lits == 1) {
    int lit = lits[0];
    Val val;
    if ((val = vals[lit]) < 0) {
      LOG ("conflict after adding falsified unit clause");
      conflict = &empty;
    } else if (!val) unit (lit);
  } else {
    if (lits >= Cls::MAXSZ) die ("maximal clause size exceeded");
    if (opts.hash && lits == 2) {
      if (!level) {
	if (hash.cntnd (lits[0], (1^lits[1]))) {
	  unit (lits[0]), stats.hash.units++;
	  lits.shrink ();
	  return 0;
	}
	if (hash.cntnd ((1^lits[0]), lits[1])) {
	  unit (lits[1]), stats.hash.units++;
	  lits.shrink ();
	  return 0;
	}
	if (hash.cntnd ((1^lits[0]), (1^lits[1])))
	  merge (lits[0], (1^lits[1]), stats.hash.merged);
      } else {
	if (hash.contains (lits[0], (1^lits[1])))
	  units.push (mem, lits[0]), stats.hash.units++;
	else if (hash.contains ((1^lits[0]), lits[1]))
	  units.push (mem, lits[1]), stats.hash.units++;
	else if (hash.contains ((1^lits[0]), (1^lits[1])))
	  merge (lits[0], (1^lits[1]), stats.hash.merged);
      }
    }
    size_t bytes = Cls::bytes (lits);
    mem += bytes;
    res = (Cls *) callocate (bytes);
    res->lnd = lnd;
    res->size = lits;
    if (lnd) {
      int * p = lits.begin (), * eol = lits.end ();
      for (int * q = p + 1; q < eol; q++)
	if (vars[*q/2].dlevel > vars[*p/2].dlevel)
          p = q;
      swap (*p, lits[0]);
      p = lits.begin () + 1;
      for (int * q = p + 1; q < eol; q++)
	if (vars[*q/2].dlevel > vars[*p/2].dlevel)
          p = q;
      swap (*p, lits[1]);
    }
    int * q = res->lits, * eol = lits.end ();
    for (const int * p = lits.begin (); p < eol; p++)
      *q++ = *p;
    *q = 0;
    if (lits == 2) res->binary = true, stats.clauses.binary++, stats.impls++;
    else if (lnd) stats.clauses.learned++;
    else stats.clauses.original++;
    connect (res);
  }
  lits.shrink ();
  return res;
}

inline void Solver::marklits () {
  for (const int * p = lits.begin (); p < lits.end (); p++) 
    vars[*p/2].mark = lit2val (*p);
}

inline void Solver::unmarklits (void) {
  for (const int * p = lits.begin (); p < lits.end (); p++) 
    vars[*p/2].mark = 0;
}

inline bool Solver::bwsub (unsigned sig, Cls * c) {
  assert (!c->trash && !c->dirty && !c->garbage);
  limit.budget.bw.sub--;
  int count = lits;
  if (c->size < (unsigned) count) return false;
  INC (sigs.bw.l1.srch);
  if (!sigsubs (sig, c->sig)) { INC (sigs.bw.l1.hits); return false; }
  int lit;
  for (int * p = c->lits; count && (lit = *p); p++) {
    Val u = lit2val (lit), v = vars[lit/2].mark;
    if (u == v) count--;
  }
  return !count;
}

int Solver::bwstr (unsigned sig, Cls * c) {
  assert (!c->trash && !c->dirty && !c->garbage);
  limit.budget.bw.str--;
  int count = lits;
  if (c->size < (unsigned) count) return 0;
  INC (sigs.bw.l1.srch);
  if (!sigsubs (sig, c->sig)) { INC (sigs.bw.l1.hits); return 0; }
  int lit, res = 0;
  for (int * p = c->lits; count && (lit = *p); p++) {
    Val u = lit2val (lit), v = vars[lit/2].mark;
    if (abs (u) != abs (v)) continue;
    if (u == -v) { if (res) return 0; res = lit; }
    count--;
  }
  assert (count >= 0);
  res = count ? 0 : res;

  return res;
}

void Solver::remove (int del, Cls * c) {
  assert (!c->lnd && !c->trash && !c->garbage && !c->dirty && !c->gate);
  assert (c->lits[0] && c->lits[1]);
  if (c->binary) {
    int pos = (c->lits[1] == del);
    assert (c->lits[pos] == del);
    unit (c->lits[!pos]);
  } else {
    disconnect (c);
    int * p = c->lits, lit;
    while ((lit = *p) != del) assert (lit), p++;
    while ((lit = *++p)) p[-1] = lit;
    p[-1] = 0;
    assert (p - c->lits >= 3);
    if (p - c->lits == 3) {
      assert (stats.clauses.original > 0);
      stats.clauses.original--;
      c->binary = true;
      stats.clauses.binary++;
      stats.impls++;
      int l0 = c->lits[0], l1 = c->lits[1];
      if (opts.hash) {
	if (hash.contains (l0, (1^l1)))
	  unit (l0), stats.hash.units++;
	else if (hash.contains ((1^l0), l1))
	  unit (l1), stats.hash.units++;
      }
    }
    setsig (c);
    connect (c);
    touch (del);
  }
}

void Solver::backward () {
  if (lits <= 1) return;
  limit.budget.bw.str += 2;
  limit.budget.bw.sub += 4;
  marklits ();
  int first = 0;
  unsigned minlen = UINT_MAX;
  for (int i = 0; i < lits; i++) {
    int other = lits[i];
    unsigned len = orgs[other];
    if (len < minlen) first = other, minlen = len;
  }
  unsigned sig = litsig ();
  assert (first);
  INC (sigs.bw.l2.srch);
  if (sigsubs (sig, bwsigs[first])) {
    for (unsigned i = 0; limit.budget.bw.sub >= 0 && i < orgs[first]; i++) {
      Cls * other = orgs[first][i];
      if (other->trash || other->dirty || other->garbage) continue;
      if (!bwsub (sig, other)) continue;
      LOG ("found another backward subsumed clause");
      stats.subs.bw++;
      limit.budget.bw.sub += 4;
      dump (other);
    }
  } else INC (sigs.bw.l2.hits);
  int second = 0;
  minlen = UINT_MAX;
  for (int i = 0; i < lits; i++) {
    int other = lits[i];
    if (other == first) continue;
    unsigned len = orgs[other];
    if (len < minlen) second = other, minlen = len;
  }
  assert (second);
  if (orgs[first^1] < minlen) second = (first^1);
  for (int round = 0; round <= 1; round++) {
    int start = round ? second : first;
    INC (sigs.bw.l2.srch);
    if (!sigsubs (sig, bwsigs[start])) { INC (sigs.bw.l2.hits); continue; }
    Orgs & org = orgs[start];
    for (unsigned i = 0; limit.budget.bw.str >= 0 && i < org; i++) {
      Cls * other = org[i];
      if (other->trash || other->dirty || other->garbage) continue;
      int del = bwstr (sig, other);
      if (!del) continue;
      LOG ("strengthening backward clause by removing " << del);
      stats.str.bw++;
      limit.budget.bw.str += 3;
      remove (del, other);
      assert (litsig () == sig);
    }
  }
  unmarklits ();
}

inline bool Solver::fwsub (unsigned sig, Cls * c) {
  assert (!c->trash && !c->dirty && !c->garbage);
  limit.budget.fw.sub--;
  INC (sigs.fw.l1.srch);
  if (!sigsubs (c->sig, sig)) { INC (sigs.fw.l1.hits); return false; }
  int lit;
  for (int * p = c->lits; (lit = *p); p++) {
    Val u = lit2val (lit), v = vars[lit/2].mark;
    if (u != v) return false;
  }
  return true;
}

inline int Solver::fwstr (unsigned sig, Cls * c) {
  assert (!c->trash && !c->dirty && !c->garbage);
  limit.budget.fw.str--;
  INC (sigs.fw.l1.srch);
  if (!sigsubs (c->sig, sig)) { INC (sigs.fw.l1.hits); return 0; }
  int res = 0, lit;
  for (int * p = c->lits; (lit = *p); p++) {
    Val u = lit2val (lit), v = vars[lit/2].mark;
    if (u == v) continue;
    if (u != -v) return 0;
    if (res) return 0;
    res = (lit^1);
  }
  return res;
}

bool Solver::forward (void) {
  if (lits <= 1) return false;
  limit.budget.fw.str += 3;
  limit.budget.fw.sub += 5;
  assert (fwds);
  marklits ();
  unsigned sig = litsig ();
  bool res = false;
  if (lits >= 2) {
    for (int i = 0; !res && limit.budget.fw.sub >= 0 && i < lits; i++) {
      int lit = lits[i];
      INC (sigs.fw.l2.srch);
      if (!(fwsigs[lit] & sig)) { INC (sigs.fw.l2.hits); continue; }
      Fwds & f = fwds[lit];
      for (unsigned j = 0; !res && limit.budget.fw.sub >= 0 && j < f; j++) {
	Cls * c = f[j];
	if (c->trash || c->dirty || c->garbage) continue;
	res = fwsub (sig, c);
      }
    }
    if (res) {
      LOG ("new clause is subsumed");
      stats.subs.fw++;
      limit.budget.fw.sub += 5;
    }
  }
  if (!res)
    for (int sign = 0; sign <= 1; sign++)
      for (int i = 0; limit.budget.fw.str >= 0 && i < lits; i++) {
	int lit = lits[i];
	INC (sigs.fw.l2.srch);
	if (!(fwsigs[lit] & sig)) { INC (sigs.fw.l2.hits); continue; }
	lit ^= sign;
	Fwds & f = fwds[lit];
	int del  = 0;
	for (unsigned j = 0; !del && limit.budget.fw.str >= 0 && j < f; j++) {
	  Cls * c = f[j];
	  if (c->trash || c->dirty || c->garbage) continue;
	  del = fwstr (sig, c);
	}
	if (!del) continue;
	assert (sign || del/2 != lit/2);
	LOG ("strengthen new clause by removing " << del);
	stats.str.fw++;
	limit.budget.fw.str += 8;
	assert (vars[del/2].mark == lit2val (del));
	vars[del/2].mark = 0;
	lits.remove (del);
	sig = litsig ();
	i = -1;
      }
  unmarklits ();
  return res;
}

void Solver::import (void) {
  bool trivial = false;
  int * q = lits.begin ();
  const int * p, * eol = lits.end ();
  for (p = lits.begin (); !trivial && p < eol; p++) {
    int lit = *p, v = lit/2;
    assert (1 <= v && v <= size);
    Val tmp = vals[lit];
    if (!tmp) {
      int prev = vars[v].mark;
      int val = lit2val (lit);
      if (prev) {
	if (val == prev) continue;
	assert (val == -prev);
	trivial = 1;
      } else {
	vars[v].mark = val;
	*q++ = lit;
      }
    } else if (tmp > 0)
      trivial = true;
  }

  while (p > lits.begin ()) vars[*--p/2].mark = 0; 

  if (!trivial) { 
    lits.shrink (q);
    clause (false); 
  } 
  lits.shrink ();
}

inline bool Solver::satisfied (Anchor<Cls> & anchor) {
  for (const Cls * p = anchor.head; p; p = p->prev) {
    int lit;
    for (const int * q = p->lits; (lit = *q) && val (lit) <= 0; q++)
      ;
    if (!lit) return false;
  }
  return true;
}

inline bool Solver::satisfied () {
  if (!satisfied (binary)) return false;
  if (!satisfied (original)) return false;
#ifdef PRECOCHECK
  for (const int * p = check.begin (); p < check.end (); p++) {
    bool found = false;
    int lit;
    while ((lit = *p)) {
      if (val (lit) > 0) found = true;
      p++;
    }
    if (!found) return false;
  }
#endif
  return true;
}

inline void Solver::prop2 (int lit) {
  assert (vals[lit] < 0);
  LOG ("prop2 " << lit);
  Occs * os = occs + lit;
  Occ * eos = os->end ();
  for (const Occ * p = os->begin (); p < eos; p++) {
    if (p->cls) continue;
    int other = p->blit;
    Val val = vals[other];
    if (val > 0) continue;
    if (!val) { imply (other, lit); continue; }
    LOG ("conflict in binary clause while propagating " << (1^lit));
    conflict = lit2conflict (dummy, lit, other);
  }
}

inline void Solver::propl (int lit) {
  assert (vals[lit] < 0);
  LOG ("propl " << lit);
  Occs & os = occs[lit];
  Occ * a = os.a, * t = a + os.n, * p = a, * q = a;
CONTINUE_OUTER_LOOP:
  while (p < t) {
    int blit = p->blit;
    Cls * cls = p++->cls;
    *q++ = Occ (blit, cls);
    if (!cls) continue;
    Val val = vals[blit];
    if (val > 0) continue;
#ifndef NSTATSPRECO
    stats.visits++;
    if (cls->lits[2] && !*(cls->lits + 3)) stats.ternaryvisits++;
#endif
    int sum = cls->lits[0]^cls->lits[1];
    int other = sum^lit;
    val = vals[other];
    q[-1].blit = other;
    if (val > 0) continue;
    int prev = lit, next, *l, *lits2 = cls->lits+2;
    for (l = lits2; (next = *l); l++) {
      *l = prev; prev = next;
      if (vals[next] < 0) continue;
      occs[next].push (mem, Occ (other, cls));
      int pos = (cls->lits[1] == lit);
      cls->lits[pos] = next;
      q--; goto CONTINUE_OUTER_LOOP;
    }
    while (l > lits2) { next = *--l; *l = prev; prev = next; }
    if (val) {
      LOG ("conflict in large clause while propagating " << (1^lit));
      conflict = cls;
      break;
    }
    force (other, cls); 
    long moved = ((char*)os.a) - (char*)a;
    if (moved) fix (p,moved), fix (q,moved), fix (t,moved), a = os.a;
  }
  while (p < t) *q++ = *p++;
  os.shrink (q - a);
}

bool Solver::bcp () {
  if (conflict) return false;
  if (!level && units) flush ();
  if (conflict) return false;
  int lit, props = 0;
  for (;;) {
    if (queue2 < trail) {
      props++;
      lit = 1^trail[queue2++];
      prop2 (lit);
    } else if (queue < trail) {
      if (conflict) break;
      lit = 1^trail[queue++];
      propl (lit);
      if (conflict) break;
    } else
      break;
  }
  stats.props += props;
  return !conflict;
}

inline int Solver::phase (Var * v) {
  int lit = 2 * (v - vars) + 1;
  if (v->phase > 0 || 
      (!v->phase && (jwhs[lit^1] > jwhs[lit]))) lit ^= 1;
  return lit;
}

void Solver::extend () {
  report (1, 't');
  extending = true;
  int n = elits;
  while (n > 0) {
    int lit = elits[--n];
    assert (!repr [lit]);
    LOG ("extending " << lit);
    while (elits[n-1]) {
      bool forced = true;
      int other;
      while ((other = elits[--n])) {
	other = find (other);
	if (other == lit) continue;
	if (other == (lit^1)) { forced = false; continue; }
	Val v = val (other);
	if (v > 0) forced = false;
      }
      if (!forced) continue;
      Val v = val (lit);
      if (v) assert (v > 0);
      else assume (lit);
    }
    if (!val (lit)) assume (lit^1);
    n--;
  }
#ifndef NDEBUG
  for (int lit = 2; lit <= 2 * size + 1; lit += 2)
    assert (val (lit));
#endif
  extending = false;
}

bool Solver::decide () {
  Rnk * r;
  for (;;) {
    if (!schedule.decide) { extend (); return false; }
    r = schedule.decide.max ();
    int idx = r - rnks;
    int lit = 2 * idx;
    if (!vars[idx].eliminated && !repr[lit] && !vals[lit]) break;
    (void) schedule.decide.pop ();
  }
  assert (r);
  stats.decisions++;
  stats.sumheight += level;
  if (opts.random && agility <= 10 * 100000 && rng.oneoutof (opts.spread)) {
    stats.random++;
    unsigned n = schedule.decide; assert (n);
    unsigned s = rng.next () % n, d = rng.next () % n;
    while (ggt (n, d) != 1) {
      d++; if (d >= n) d = 1;
    }
    for (;;) {
      r = schedule.decide[s];
      int lit = 2 * (r - rnks);
      if (!vars[lit/2].eliminated && !repr[lit] && !vals[lit]) break;
      s += d; if (s >= n) s -= n;
    }
  }
  int lit = phase (var (r));
  assert (!vals[lit]);
  LOG ("decision " << lit);
  assume (lit);
  iterating = false;
  return true;
}

inline bool Solver::min2 (int lit, int other, int depth) {
  assert (lit != other && vals[lit] > 0);
  if (vals[other] >= 0) return false;
  Var * v = vars + (lit/2);
  if (v->binary && v->reason.lit == other) {
    Var * d = vars + (v->dominator/2);
    assert (d != v);
    assert (depth);
    if (d->removable) return true;
    if (d->mark) return true;
  }
  Var * u = vars + (other/2);
  assert (opts.minimize > Opts::RECUR || u->tlevel < v->tlevel);
  if (u->tlevel > v->tlevel) return false;
  if (u->mark) return true;
  if (u->removable) return true;
  if (u->unremovable) return false;
  return minimize (u, depth);
}

inline bool Solver::minl (int lit, Cls * cls, int depth) {
  assert (vals[lit] > 0);
  assert (cls->lits[0] == lit || cls->lits[1] == lit);
  Var * v = vars + (lit/2);
  int other;
  for (const int * p = cls->lits; (other = *p); p++) {
    if (other == lit) continue;
    if (vals[other] >= 0) return false;
    Var * u = vars + (other/2);
    assert (opts.minimize > Opts::RECUR || u->tlevel < v->tlevel);
    if (u->tlevel > v->tlevel) return false;
    if (u->mark) continue;
    if (u->removable) continue;
    if (u->unremovable) return false;
    int l = u->dlevel;
    if (!l) continue;
    if (!frames[l].pulled) return false;
  }
  for (const int * p = cls->lits; (other = *p); p++) {
    if (other == lit) continue;
    Var * u = vars + other/2;
    if (u->mark) continue;
    if (u->removable) continue;
    if (u->unremovable) return false;
    int l = u->dlevel;
    if (!l) continue;
    if (!minimize (u, depth)) return false;
  }
  return true;
}

inline bool Solver::strengthen (int lit, int depth) {
  assert (vals[lit] > 0);
  assert (opts.minimize >= Opts::STRONG);
  if (vars[lit/2].dlevel != jumplevel || lits > 10) return false;
  Occs & os = occs[lit];
  Occ * eos = os.end ();
  int bound = opts.strength;
  for (Occ * o = os.begin (); o < eos && bound-- > 0; o++)
    if (!o->cls && min2 (lit, o->blit, depth)) return true;
    else if (o->cls && minl (lit, o->cls, depth)) return true;
  return false;
}

bool Solver::minimize (Var * v, int depth) {
  if (depth > stats.mins.depth) stats.mins.depth = depth;
  assert (v->dlevel != level);
  if (v->removable) return true;
  if (depth && v->mark) return true;
  if (v->unremovable) return false;
  int l = v->dlevel;
  if (!l) return true;
  if (opts.minimize == Opts::NONE) return false;
  if (depth && opts.minimize == Opts::LOCAL) return false;
  if (!v->binary && !v->reason.cls) return false;
  if (!frames[l].pulled) return false;
  if (depth++ >= opts.maxdepth) return false;
  assert (!v->onstack);
  v->onstack = true;
  bool res = false;
  int lit = 2 * (v - vars);
  Val val = vals[lit];
  if (val < 0) lit ^= 1;
  assert (vals[lit] > 0);
  if (v->binary) res = min2 (lit, v->reason.lit, depth);
  else res = minl (lit, v->reason.cls, depth);
  if (!res && opts.minimize >= Opts::STRONG)
    if ((res = strengthen (lit, depth))) 
      stats.mins.strong++;
  v->onstack = false;
  if (res) v->removable = true;
  else v->unremovable = true;
  seen.push (mem, v);
  return res;
}

void Solver::undo (int newlevel, bool save) {
  assert (newlevel <= level);
  if (newlevel == level) return;
  if (save) saved.shrink ();
  int tlevel = frames[newlevel+1].tlevel;
  while (tlevel < trail) {
    int lit = trail.pop ();
    assert (vals[lit] > 0);
    vals[lit] = vals[lit^1] = 0;
    LOG("unassign " << lit);
    if (!repr[lit]) {
      if (save) saved.push (mem, lit);
      Rnk & r = rnks[lit/2];
      if (!schedule.decide.contains (&r)) schedule.decide.push (mem, &r);
    }
    int idx = lit/2;
    Var & v = vars[idx];
    if (v.binary) continue;
    Cls * c = v.reason.cls;
    if (!c) continue;
    c->locked = false;
    if (!c->lnd) continue;
    assert (stats.clauses.locked);
    stats.clauses.locked--;
  }
  frames.shrink (newlevel + 1);
  level = newlevel;
  queue = queue2 = trail;
  conflict = 0;
}

inline int & Solver::iirf (Var * v, int t) {
  assert (1 <= t && t < opts.order && 2 <= opts.order);
  return iirfs[(opts.order - 1) * (v - vars) + (t - 1)];
}

inline void Solver::bump (Var * v) {
  assert (stats.conflicts > 0);
  int inc = hinc;
  if (opts.order >= 2) {
    int h, s, w, c = stats.conflicts - 1;
    h = s = w = (1 << (opts.order - 1));
    for (int i = 1; i <= opts.order - 1; i++) {
      w >>= 1;
      s += w;
      for (int j = 1; j <= opts.order - 1; j++) {
	int d = iirf (v, j);
	if (d > c) continue;
	if (d == c) h += w;
	break;
      }
    }
    assert (w == 1 && s == (1 << opts.order) - 1);
    assert (opts.order <= h && h <= s);
    for (int i = opts.order - 1; i > 1; i--)
      iirf (v, i) = iirf (v, i - 1);
    iirf (v, 1) = stats.conflicts;
    inc = h * ((inc + (s-1)) / s);
  }
  Rnk * r = rnk (v);
  r->heat += inc;
  schedule.decide.up (r);
  assert (hinc < (1<<28));
  if (r->heat >= (1<<24)) {
    stats.rescored++;
    hinc >>= 14;
    for (Rnk * s = rnks + 1; s <= rnks + size; s++)
      s->heat >>= 14;
  }
}

inline void Solver::pull (int lit) {
  Var * v = vars + (lit/2);
  if (!v->dlevel || v->mark) return;
  LOG ("resolve " << lit << " open " << open);
  v->mark = 1; 
  seen.push (mem, v);
  if (v->dlevel == level) open++;
  else {
    lits.push (mem, lit);
    if (!frames[v->dlevel].pulled) {
      frames[v->dlevel].pulled = true;
      levels.push (mem, v->dlevel);
    }
  }
}

bool Solver::analyze () {
  assert (conflict);
  stats.conflicts++;
  if (!level) return false;
  assert (!lits); assert (!pulled); assert (!seen);
  int i = trail, uip = 0, lit;
  open = 0;
  Var * u = 0;
  do {
    if (!u || !u->binary) {
      Cls * r = (u ? u->reason.cls : conflict); assert (r);
      if (r != &dummy && r->lnd) pulled.push (mem, r);
      for (const int * p = r->lits; (lit = *p); p++)
	pull (lit);
    } else {
      lit = u->reason.lit;
      pull (lit);
    }

    for (;;) {
      assert (i > 0);
      uip = trail[--i];
      u = vars + (uip/2);
      if (!u->mark) continue;
      assert (u->dlevel == level);
      assert (open > 0);
      open--;
      break;
    }
  } while (open);
  assert (uip);
  LOG ("uip " << uip);
  uip ^= 1;
  lits.push (mem, uip);

#ifndef NLOGPRECO
  printf ("c LOG 1st uip clause");
  for (const int * p = lits.begin (); p < lits.end (); p++)
    printf (" %d", *p);
  fputc ('\n', stdout);
#endif

  Var ** bos = seen.begin ();
  for (Var ** p = seen.end (); p > bos; p--)
    bump (p[-1]);

  while (pulled) {
    Cls * r = pulled.pop ();
    if (!r) continue;
    mtf (learned, r);
  }

  jumplevel = 0;
  int * eolevels = levels.end (), l;
  for (const int * p = levels.begin (); p < eolevels; p++)
    if ((l = *p) && l < level && l > jumplevel) jumplevel = l;

#ifndef NDEBUG
  for (Var **p = seen.begin (); p < seen.end (); p++) {
    Var * v = *p; assert (v->mark && !v->removable && !v->unremovable);
  }
#endif
  int * q = lits.begin (), * eolits = lits.end ();
  for (const int * p = q; p < eolits; p++) {
    lit = *p; Var * v = vars + (lit/2);
    assert ((v->dlevel == level) == (lit == uip));
    if (v->dlevel < level && minimize (v, 0)) { 
      LOG ("removing 1st uip literal " << lit);
      stats.mins.deleted++;
    } else *q++ = lit;
  }
  lits.shrink (q);

  Var ** eos = seen.end ();
  for (Var ** p = seen.begin(); p < eos; p++) {
    Var * v = *p; v->mark = 0; v->removable = v->unremovable = false;
  }
  seen.shrink ();

  assert (eolevels == levels.end ());
  for (const int * p = levels.begin (); p < eolevels; p++) {
    if (!(l = *p)) continue;
    assert (frames[l].pulled); 
    frames[l].pulled = false;
  }
  levels.shrink ();

  stats.lits.added += lits;

#ifndef NDEBUG
  for (const int * p = lits.begin (); p < lits.end (); p++)
    assert (*p == uip || vars[*p/2].dlevel < level);
#endif
#ifndef NLOGPRECO
  if (jumplevel + 1 < level) LOG ("backjump to " << jumplevel);
  else LOG ("backtrack to " << jumplevel);
#endif
  undo (jumplevel);
  if (!jumplevel) iterating = true;

  Cls * cls = clause (true);
  if (cls) {
    if (cls->binary) {
      assert (cls->lits[0] == uip);
      imply (uip, cls->lits[1]);
    } else force (uip, cls);
  }

  int count = 4;
  Cls * ctc = learned.tail;
  while (count-- >= 0 && ctc && recycling () && ctc != cls) {
    Cls * next = ctc->next;
    assert (!ctc->binary);
    if (!ctc->locked) recycle (ctc);
    ctc = next;
  }

  long long tmp = hinc;
  tmp *= 11; tmp /= 10;
  assert (tmp <= UINT_MAX);
  hinc = tmp;
  return true;
}

int Solver::luby (int i) {
  int k;
  for (k = 1; k < 32; k++)
    if (i == (1 << k) - 1)
      return 1 << (k-1);

  for (k = 1;; k++)
    if ((1 << (k-1)) <= i && i < (1 << k) - 1)
      return luby (i - (1u << (k-1)) + 1);
}

inline double Stats::height () {
  return decisions ? sumheight / (double) decisions : 0.0;
}

void Solver::report (bool v, char type) {
  if (v && !opts.verbose) return;
  if (!(stats.reports++ % 19)) 
    printf (
"c\n"
"c   "
"    original/binary      fixed     eliminated      learned       agility"
"\n"
"c   "
"seconds        variables     equivalent    conflicts       height       MB"
"\n"
"c\n");
  assert (size >= stats.vars.fixed + stats.vars.equiv + stats.vars.elim);
  printf ("c %c %6.1f %8d %7d %6d %5d %6d %7d %6d %6.1f %3.0f %4.0f\n",
          type,
          stats.seconds (),
	  stats.clauses.original + stats.clauses.binary,
	  size - stats.vars.fixed - stats.vars.equiv - stats.vars.elim,
	  stats.vars.fixed, stats.vars.equiv, 
	  stats.vars.elim,
	  stats.conflicts,
	  (type=='l' || type=='+' || type == '-') ?
	    limit.reduce.learned : stats.clauses.learned,
	  stats.height (),
	  agility / 100000.0,
	  mem / (double) (1<<20));
  fflush (stdout);
}

void Solver::restart () {
  int skip = agility >= opts.skip * 100000;
  if (skip) stats.restart.skipped++;
  else stats.restart.count++;
  limit.restart.conflicts = stats.conflicts;
  int delta = opts.restartint * luby (++limit.restart.lcnt);
  limit.restart.conflicts += delta;
  if (!skip) undo (0);
  if (stats.restart.maxlubydelta < delta) {
    stats.restart.maxlubydelta = delta;
    report (0, skip ? 'N' : 'R');
  } else
    report (1, skip ? 'n' :'r');
}

void Solver::rebias () {
  limit.rebias.conflicts = stats.conflicts;
  int delta = opts.rebiasint * luby (++limit.rebias.lcnt);
  limit.rebias.conflicts += delta;
  if (!opts.rebias) return;
  stats.rebias.count++;
  for (Var * v = vars + 1; v <= vars + size; v++) v->phase = 0;
  jwh ();
  if (stats.rebias.maxlubydelta < delta) {
    stats.rebias.maxlubydelta = delta;
    report (0, 'B');
  } else
    report (1, 'b');
}

inline int Solver::redundant (Cls * cls) {
  assert (!level);
  assert (!cls->locked);
  assert (!cls->garbage);
#ifndef NDEBUG
  for (const int * p = cls->lits; *p; p++) {
    Var * v = vars + (*p/2);
    assert (!v->mark);
    assert (!v->eliminated);
  }
#endif
  int res = 0, * p, lit;
#ifdef PRECOCHECK
  bool shrink = false;
  for (p = cls->lits; !res && (lit = *p); p++) {
    int other =  find (lit);
    Var * v = vars  + (other/2);
    assert (!v->eliminated);
    Val val = vals[other];
    if (val > 0 && !v->dlevel) res = 1;
    else if (val < 0 && !v->dlevel) shrink = true;
    else {
      val = lit2val (other);
      if (v->mark == val) { shrink = true; continue; }
      else if (v->mark == -val) res = 1;
      else v->mark = val;
    }
  }
  while (p > cls->lits) vars[find (*--p)/2].mark = 0;
#ifndef NDEBUG
  for (p = cls->lits; *p; p++) assert (!vars[find (*p)/2].mark);
#endif
  if (shrink || res) {
    for (p = cls->lits; *p; p++) check.push (mem, *p);
    check.push (mem, 0);
  }
  if (res) return res;
#endif

  p = cls->lits; int * q = p;
  while (!res && (lit = *p)) {
    p++;
    int other = find (lit);
    Var * v = vars + (other/2);
    assert (!v->eliminated);
    Val val = vals[other];
    if (val && !v->dlevel) {
      if (val > 0) res = 1;
    } else {
      val = lit2val (other);
      if (v->mark == val) continue;
      if (v->mark == -val) res = 1;
      else { assert (!v->mark); v->mark = val; *q++ = other; }
    }
  }
  *q = 0;
  if (!res) assert (!*q && !*p);

  int newsize = q - cls->lits;
  while (q-- > cls->lits) {
    assert (vars[*q/2].mark); vars[*q/2].mark = 0;
  }

  if (res) return res;

  setsig (cls);

  res = newsize <= 1;
  if (!newsize) conflict = &empty;
  else if (newsize == 1) { 
    LOG ("learned clause " << cls->lits[0]);
    unit (cls->lits[0]);
  } else if (newsize == 2 && !cls->binary && 2 < cls->size) {
    int l0 = cls->lits[0], l1 = cls->lits[1];
    if (opts.hash && hash.contains (l0, (1^l1)))
      unit (l0), res = 1, stats.hash.units++;
    else if (opts.hash && hash.contains ((1^l0), l1))
      unit (l1), res = 1, stats.hash.units++;
    else {
      if (cls->lnd) assert (stats.clauses.learned), stats.clauses.learned--;
      else assert (stats.clauses.original), stats.clauses.original--;
      stats.clauses.binary++;
      cls->binary = true;
      push (binary, cls);
      stats.impls++;
      res = -1;
    }
  } else assert (!res);

  return res;
}

void Solver::decompose () {
  assert (!level);
  if (!opts.decompose) return;
  report (1, 'd');
  int n = 0;
  size_t bytes = 2 * (size + 1) * sizeof (SCC);
  SCC * sccs = (SCC*) callocate (bytes); mem += bytes;
  Stack<int> work;
  saved.shrink ();
  int dfsi = 0;
  LOG ("starting scc decomposition");
  for (int root = 2; !conflict && root <= 2*size + 1; root++) {
    if (sccs[root].idx) { assert (sccs[root].done); continue; }
    assert (!work);
    work.push (mem, root);
    LOG ("new scc root " << root);
    while (!conflict && work) {
      int lit = work.top ();
      if (sccs[lit^1].idx && !sccs[lit^1].done) {
	Val val = vals[lit];
	if (val < 0) {
	  LOG ("conflict while learning unit in scc");
	  conflict = &empty;
	} else if (!val) {
	  LOG ("learned clause " << lit); unit (lit); stats.sccs.fixed++;
	}
      }
      if (conflict) break;
      Occs & os = occs[1^lit];
      unsigned i = os;
      if (!sccs[lit].idx) {
	assert (!sccs[lit].done);
	sccs[lit].idx = sccs[lit].min = ++dfsi;
	saved.push (mem, lit);
	while (i) {
	  Occ & o = os[--i];
	  if (o.cls) continue;
	  int other = o.blit;
	  if (sccs[other].idx) continue;
	  work.push (mem, other);
	}
      } else {
	work.pop ();
	if (!sccs[lit].done) {
	  while (i) {
	    Occ & o = os[--i];
	    if (o.cls) continue;
	    int other = o.blit;
	    if (sccs[other].done) continue;
	    sccs[lit].min = min (sccs[lit].min, sccs[other].min);
	  }
	  SCC * scc = sccs + lit;
	  unsigned min = scc->min;
	  if (min != scc->idx) { assert (min < scc->idx); continue; }
	  n++; LOG ("new scc " << n);
	  int m = 0, prev = 0, other = 0;
	  while (!conflict && other != lit) {
	    other = saved.pop ();
	    sccs[other].done = true;
	    LOG ("literal " << other << " added to scc " << n);
	    if (prev) {
	      int a = find (prev), b = find (other);
	      if (a == b) continue;
	      if (a == (1^b)) {
		LOG ("conflict while merging scc");
		conflict = &empty;
	      } else {
		merge (prev, other, stats.sccs.merged);
		m++;
	      }
	    } else prev = other;
	  }
	  if (m) stats.sccs.nontriv++;
	} 
      }
    }
  }

  LOG ("found " << n << " sccs");
  assert (conflict || dfsi <= 2 * size);
  work.release (mem);
  mem -= bytes;
  free (sccs);
  recompeqstats ();
}

void Solver::disconnect () {
  cleangate (), cleantrash ();
  for (int lit = 2; lit <= 2*size + 1; lit++)
    occs[lit].release (mem);
  assert (!orgs && !fwds && !bwsigs && !fwsigs);
  hash.release (mem);
}

void Solver::connect (Anchor<Cls> & anchor, bool orgonly) {
  for (Cls * p = anchor.tail; p; p = p->next) {
    assert (&anchor == &this->anchor (p));
    if (!orgonly || !p->lnd) connect (p);
  }
}

void Solver::gc (Anchor<Cls> & anchor, const char * type) {
  Cls * p = anchor.tail;
  int collected = 0;
  anchor.head = anchor.tail = 0;
  while (p) {
    Cls * next = p->next;
#ifndef NDEBUG
    p->next = p->prev = 0;
#endif
    int red = 1;
    if (!p->garbage) {
      red = redundant (p);
      if (red > 0) p->garbage = true;
    }
    if (p->garbage) { collect (p); collected++; }
    else if (!red) push (anchor, p);
    else assert (connected (binary, p));
    p = next;
  }
#ifndef NLOGPRECO
  LOG ("collected " << collected << ' ' << type << " clauses");
#else
  (void) type;
#endif
}

inline void Solver::checkclean () {
#ifndef NDEBUG
  if (opts.check) {
    for (int i = 0; i <= 2; i++) {
      Cls * p;
      if (i == 0) p = original.head;
      if (i == 1) p = binary.head;
      if (i == 2) p = learned.head;
      while (p) {
	for (int * q = p->lits; *q; q++) {
	  int lit = *q;
	  assert (!repr[lit]);
	  assert (!vals[lit]);
	  Var * v = vars + (lit/2);
	  assert (!v->eliminated);
	}
	p = p->prev;
      }
    }
  }
#endif
}

void Solver::gc () {
#ifndef NLOGPRECO
  size_t old = stats.collected;
#endif
  report (1, 'c');
  undo (0);
  disconnect ();
  gc (binary, "binary");
  gc (learned, "learned");
  gc (original, "original");
  connect (binary);
  connect (original);
  connect (learned);
  checkclean ();
#ifndef NLOGPRECO
  size_t bytes = stats.collected - old;
  LOG ("collected " << bytes << " bytes");
  print ();
#endif
}

inline int Solver::recyclelimit () const {
  return limit.reduce.learned + stats.clauses.locked;
}

inline bool Solver::recycling () const {
  return stats.clauses.learned >= recyclelimit ();
}

void Solver::reduce () {
  assert (!conflict);
  assert (trail == queue);
  if (limit.reduce.delay > 0) { limit.reduce.delay--; return; }
  stats.reductions++;
  int count = (stats.clauses.learned - stats.clauses.locked)/2;
  bool recycled;
  Cls * next;
  for (Cls * p = learned.tail; p; p = next) {
    next = p->next;
    if (p->locked) continue;
    p->garbage = true;
    recycled = true;
    if (!count--) break;
  }
  gc ();
  jwh ();
  if (!recycled) {
    limit.reduce.delay = 100 * (1 << limit.reduce.logd);
    if (limit.reduce.logd < 4) limit.reduce.logd++;
    report (1, '=');
  } else {
    limit.reduce.delay = limit.reduce.logd = 0;
    report (1, '/');
  }
}

void Solver::recompeqstats () {
  assert (!level);
  int fixed = 0, equivalent = 0, eliminated = 0;
  for (Var * v = vars + 1; v <= vars + size; v++) {
    int lit = 2 * (v - vars);
    if (vals[lit]) fixed++;
    else if (repr[lit]) equivalent++;
    else if (v->eliminated) eliminated++;
  }
  assert (fixed == stats.vars.fixed);
  assert (eliminated == stats.vars.elim);
  stats.vars.equiv = equivalent;
}

inline void Solver::checkeliminated () {
#ifndef NDEBUG
  if (opts.check) {
    for (int i = 0; i <= 2; i++) {
      Cls * p;
      if (i == 0) p = original.head;
      if (i == 1) p = binary.head;
      if (i == 2) p = learned.head;
      while (p) {
	for (int * q = p->lits; *q; q++) {
	  int lit = *q;
	  Var * v = vars + (lit/2);
	  assert (!v->eliminated);
	}
	p = p->prev;
      }
    }
  }
#endif
}

void Solver::probe () {
  assert (!conflict);
  assert (queue2 == trail);
  undo (0);
  stats.probe.phases++;
  measure = false;
  for (const Rnk * r = rnks + 1; r <= rnks + size; r++) {
    Rnk * p = prb (r);
    int lit = 2 * (r - rnks);
    int old_heat = p->heat;
    int new_heat = jwhs[lit] + jwhs[lit^1];
    if (new_heat < 0) new_heat = INT_MAX;
    p->heat = new_heat;
    if (schedule.probe.contains (p)) {
      if (new_heat > old_heat) schedule.probe.up (p);
      else if (new_heat < old_heat) schedule.probe.down (p);
    }
  }
  long long bound = opts.probeint;
  if (!stats.probe.rounds) bound *= 4;
  bound += stats.props;
  report (1, 'p');
  int fxd = stats.vars.fixed;
  int last = -1, filled = 0;
  if (!schedule.probe) {
FILL:
    filled++;
    for (Rnk * p = prbs + 1; p <= prbs + size; p++)
      schedule.probe.push (mem, p);
  }
  while (stats.props < bound && !conflict) {
    assert (!level);
    if (!schedule.probe) {
      stats.probe.rounds++;
      if (last == filled) goto FILL;
      break;
    }
    Rnk * p = schedule.probe.pop ();
    int lit = 2 * (p - prbs);
    if (vals[lit]) continue;
    if (repr[lit]) continue;
    if (vars[lit/2].eliminated) continue;
    assert (!level);
    long long old = stats.props;
    LOG ("probing " << lit);
    stats.probe.variables++;
    assume (lit);
    bool ok = bcp ();
    undo (0, ok);
    if (!ok) goto FAILEDLIT;
    lit ^= 1;
    LOG ("probing " << lit);
    assume (lit);
    stats.probe.variables++;
    if (!bcp ()) { undo (0); goto FAILEDLIT; }
    {
      int * q = saved.begin (), * eos = saved.end ();
      for (const int * p = q; p < eos; p++) {
	int other = *p;
	if (other == (lit^1)) continue;
	if (vals[other] < 0) merge (lit, other^1, stats.probe.merged);
	if (vals[other] <= 0) continue;
	*q++ = other;
      }
      undo (0);
      if (q == saved.begin ()) continue;
      saved.shrink (q);
      for (const int * p = saved.begin (); p < q; p++) {
	stats.probe.lifted++;
	last = filled;
	int other = *p;
	LOG ("lifting " << other);
	LOG ("learned clause " << other);
	unit (other);
      }
    }
    goto BCPANDINCBOUND;
FAILEDLIT:
    stats.probe.failed++;
    last = filled;
    LOG ("learned clause " << (1^lit));
    unit (1^lit);
BCPANDINCBOUND:
    bcp ();
    bound += stats.props - old;
    bound += 1000; // TODO less?
  }
  limit.props.probe = stats.props + opts.probeprd*(long long)opts.probeint;
  limit.fixed.probe = stats.vars.fixed;
  if (fxd < stats.vars.fixed) report (1, 'f');
  measure = true;
  recompeqstats ();
}

bool Solver::andgate (int lit) {
  // TODO L2 sigs?
  assert (!vars[lit/2].eliminated);
  assert (!gate && !gatestats);
  if (!opts.subst) return false;
  if (!opts.ands) return false;
  if (!orgs[lit] || !orgs[1^lit]) return false;
  Cls * b = 0;
  int other, notlit = (1^lit);
  for (unsigned i = 0; !b && i < orgs[lit]; i++) {
    Cls * c = orgs[lit][i];
    if (!sigsubs (c->sig, bwsigs[lit^1])) continue;
    assert (!gate);
    gcls (c);
    gatelen = 0;
    bool hit = false;
    for (const int * p = c->lits; (other = *p); p++) {
      if (lit == other) { hit = true; continue; }
      assert (!vars[other/2].mark);
      vars[other/2].mark = -lit2val (other);
      gatelen++;
    }
    assert (hit);
    assert (gatelen);
    unsigned count = gatelen;
    for (unsigned j = 0; j < orgs[notlit]; j++) {
      if (orgs[notlit] - j < count) break;
      Cls * d = orgs[notlit][j];
      if (d->lits[2]) continue;
      int pos = (d->lits[1] == notlit);
      assert (d->lits[pos] == notlit);
      assert (d->binary);
      other = d->lits[!pos];
      if (vars[other/2].mark != lit2val (other)) continue;
      vars[other/2].mark = 0;
      gcls (d);
      if (!--count) break;
    }
    for (const int * p = c->lits; (other = *p); p++)
      vars[other/2].mark = 0;
    if (count) cleangate ();
    else b = c;
  }
  if (!b) { assert (!gate); return false; }
  assert (gate == gatelen + 1);
  gatestats = (gatelen >= 2) ? &stats.subst.ands : &stats.subst.nots;
  gatepivot = lit;
  posgate = 1;
#ifndef NLOGPRECO
  printf ("c LOG %s gate %d = ", (gatelen >= 2) ? "and" : "not", lit);
  for (const int * p = b->lits; (other = *p); p++) {
    if (other == lit) continue;
    printf ("%d", (1^other));
    if (p[1] && (p[1] != lit || p[2])) fputs (" & ", stdout);
  }
  fputc ('\n', stdout);
  printgate ();
#endif
  return true;
}

bool Solver::xorgate (int lit) {
  // TODO L2 sigs
  assert (!vars[lit/2].eliminated);
  assert (!gate && !gatestats);
  if (!opts.subst) return false;
  if (!opts.xors) return false;
  if (orgs[lit] < 2 || orgs[1^lit] < 2) return false;
  if (orgs[lit] > orgs[1^lit]) lit ^= 1;
  Cls * b = 0;
  int len = 0, other;
  for (unsigned i = 0; i < orgs[lit] && !b; i++) {
    Cls * c = orgs[lit][i];
    const int maxlen = 20;
    assert (maxlen < 31);
    if (c->size > maxlen) continue;
    if (!c->lits[2]) continue;
    assert (!c->binary);
    if (!sigsubs (c->sig, bwsigs[lit^1])) continue;
    int * p;
    for (p = c->lits; *p; p++)
      ;
    len = p - c->lits;
    assert (len >= 3);
    unsigned required = (1u << (len-1));
    for (p = c->lits; (other = *p); p++) {
      if (other == lit) continue;
      if (orgs[other] < required) break;
      if (orgs[other^1] < required) break;
      if (!sigsubs (c->sig, bwsigs[other])) break;
      if (!sigsubs (c->sig, bwsigs[1^other])) break;
    }
    if (other) continue;
    assert (!gate);
    assert (0 < len && len <= maxlen);
    unsigned signs;
    for (signs = 0; signs < (1u<<len); signs++) {
      if (parity (signs)) continue;
      int start = 0, startlen = INT_MAX, s = 0;
      for (p = c->lits; (other = *p); p++, s++) {
	if ((signs & (1u<<s))) other ^= 1;
	int tmp = orgs[other];
	if (start && tmp >= startlen) continue;
	startlen = tmp;
	start = other;
      }
      assert (s == len && start && startlen < INT_MAX);
      unsigned j;
      Cls * found = 0;
      for (j = 0; !found && j < orgs[start]; j++) {
	Cls * d = orgs[start][j];
	if (d->sig != c->sig) continue;
	for (p = d->lits; *p; p++)
	  ;
	if (p - d->lits != len) continue;
	bool hit = false;
	s = 0;
	for (p = c->lits; (other = *p); p++, s++) {
	  if ((signs & (1u<<s))) other ^= 1;
	  if (other == start) { hit = true; continue; }
	  if (!d->contains (other)) break;
	}
	assert (other || hit);
	if (!other) found = d;
      }
      assert (signs || found);
      if (!found) break;
      assert (required);
      required--;
      gcls (found);
    } 
    if (signs == (1u<<len)) { assert (!required); b = c; }
    else cleangate ();
  }
  if (!b) { assert (!gate); return false; }
  assert (len >= 3);
  assert (gate == (1<<(len-1)));
  gatepivot = lit;
  gatestats = &stats.subst.xors;
  gatelen = len - 1;
  int pos = -1, neg = gate;
  for (;;) {
    while (pos+1 < neg && gate[pos+1]->contains (lit)) pos++;
    assert (pos >= gate || gate[pos+1]->contains(1^lit));
    if (pos+1 == neg) break;
    while (pos < neg-1 && gate[neg-1]->contains ((1^lit))) neg--;
    assert (neg < 0 || gate[neg-1]->contains(lit));
    if (pos+1 == neg) break;
    assert (pos < neg);
    swap (gate[++pos], gate[--neg]);
  }
  posgate = pos + 1;
#ifndef NLOGPRECO
  printf ("c LOG %d-ary xor gate %d = ", len-1, (lit^1));
  for (const int * p = b->lits; (other = *p); p++) {
    if (other == lit) continue;
    printf ("%d", other);
    if (p[1] && (p[1] != lit || p[2])) fputs (" ^ ", stdout);
  }
  fputc ('\n', stdout);
  printgate ();
#endif
  return true;
}

Cls * Solver::find (int a, int b, int c) {
  unsigned sig = listig (a) | listig (b) | listig (c);
  unsigned all = bwsigs[a] & bwsigs[b] & bwsigs[c];
  if (!sigsubs (sig, all)) return 0;
  int start = a;
  if (orgs[start] > orgs[b]) start = b;
  if (orgs[start] > orgs[c]) start = c;
  for (unsigned i = 0; i < orgs[start]; i++) {
    Cls * res = orgs[start][i];
    if (res->sig != sig) continue;
    assert (res->lits[0] && res->lits[1]);
    if (!res->lits[2]) continue;
    assert (!res->binary);
    if (res->lits[3]) continue;
    int l0 = res->lits[0], l1 = res->lits[1], l2 = res->lits[2];
    if ((a == l0 && b == l1 && c == l2) ||
        (a == l0 && b == l2 && c == l1) ||
        (a == l1 && b == l0 && c == l2) ||
        (a == l1 && b == l2 && c == l0) ||
        (a == l2 && b == l0 && c == l1) ||
        (a == l2 && b == l1 && c == l2)) return res;
  }
  return 0;
}

int Solver::itegate (int lit, int cond, int t) {
  Cls * c = find (lit^1, cond, t^1);
  if (!c) return 0;
  int start = lit, nc = (cond^1);;
  if (orgs[nc] < orgs[start]) start = nc;
  unsigned sig = listig (lit) | listig (nc);
  for (unsigned i = 0; i < orgs[start]; i++) {
    Cls * d = orgs[start][i];
    if (!sigsubs (sig, d->sig)) continue;
    assert (d->lits[0] && d->lits[1]);
    if (!d->lits[2]) continue;
    assert (!d->binary);
    if (d->lits[3]) continue;
    int l0 = d->lits[0], l1 = d->lits[1], l2 = d->lits[2], res;
         if (l0 == lit && l1 == nc) res = l2;
    else if (l1 == lit && l0 == nc) res = l2;
    else if (l0 == lit && l2 == nc) res = l1;
    else if (l2 == lit && l0 == nc) res = l1;
    else if (l1 == lit && l2 == nc) res = l0;
    else if (l2 == lit && l1 == nc) res = l0;
    else continue;
    Cls * e = find (lit^1, nc, res^1);
    if (!e) continue;
    gcls (d);
    gcls (c);
    gcls (e);
    return res;
  }
  return 0;
}

bool Solver::itegate (int lit) {
  // TODO L2 sigs
  assert (!vars[lit/2].eliminated);
  assert (!gate && !gatestats);
  if (!opts.subst) return false;
  if (!opts.ites) return false;
  if (orgs[lit] < 2 || orgs[1^lit] < 2) return false;
  if (orgs[lit] > orgs[1^lit]) lit ^= 1;
  Cls * b = 0;
  int cond = 0, t = 0, e = 0;
  for (unsigned i = 0; i < orgs[lit] && !b; i++) {
    Cls * c = orgs[lit][i];
    assert (c->lits[0] && c->lits[1]);
    if (!c->lits[2]) continue;
    assert (!c->binary);
    if (c->lits[3]) continue;
    assert (!gate);
    int o0, o1, l0 = c->lits[0], l1 = c->lits[1], l2 = c->lits[2];
    if (lit == l0) o0 = l1, o1 = l2;
    else if (lit == l1) o0 = l0, o1 = l2;
    else assert (lit == l2), o0 = l0, o1 = l1;
    assert (!gate);
    gcls (c);
    if ((e = itegate (lit, cond = o0, t = o1)) ||
        (e = itegate (lit, cond = o1, t = o0))) b = c;
    else cleangate ();
  }
  if (!b) { assert (!gate); return false; }
  assert (cond && t && e);
  assert (gate == 4);
  gatestats = &stats.subst.ites;
  gatepivot = lit;
  posgate = 2;
#ifndef NLOGPRECO
  LOG ("ite gate " << lit << " = " << 
       (1^cond) << " ? " << (1^t) << " : " << (1^e));
  printgate ();
#endif
  return true;
}

bool Solver::resolve (Cls * c, int pivot, Cls * d, bool tryonly) {
  assert (tryonly || (c->dirty && d->dirty));
  assert (tryonly != vars[pivot/2].eliminated);
  assert (!vals[pivot] && !repr[pivot]);
  if (gate && c->gate == d->gate) return false;
  stats.elim.resolutions++;
  int other, notpivot = (1^pivot);
  bool found = false, res = true;
  const int * p;
  assert (!lits);
  for (p = c->lits; (other = *p); p++) {
    if (other == pivot) { found = true; continue; }
    assert (other != notpivot);
    other = find (other);
    if (other == pivot) continue;
    if (other == notpivot) { res = false; goto DONE; }
    Val v = vals [other];
    if (v < 0) continue;
    if (v > 0) { res = false; goto DONE; }
    v = vars[other/2].mark;
    Val u = lit2val (other);
    if (!v) {
      vars[other/2].mark = u;
      lits.push (mem, other);
    } else if (v != u) { res = false; goto DONE; }
  }
  assert (found);
  found = false;
  for (p = d->lits; (other = *p); p++) {
    if (other == notpivot) { found = true; continue; }
    assert (other != pivot);
    other = find (other);
    if (other == notpivot) continue;
    if (other == pivot) { res = false; goto DONE; }
    Val v = vals[other];
    if (v < 0) continue;
    if (v > 0) { res = false; goto DONE; }
    v = vars[other/2].mark;
    Val u = lit2val (other);
    if (!v) {
      vars[other/2].mark = u;
      lits.push (mem, other);
    } else if (v != u) { res = false; goto DONE; }
  }
  assert (found);
DONE:
  for (p = lits.begin (); p < lits.end (); p++) {
    Var * u = vars + (*p/2);
    assert (u->mark);
    u->mark = 0;
  }
  if (res) {
    if (!lits) {
      LOG ("conflict in resolving clauses");
      conflict = &empty;
      res = false;
    } else if (lits == 1) {
      LOG ("learned clause " << lits[0]);
      unit (lits[0]);
      res = false;
    } else if (!tryonly) {
      if (!forward ()) {
	backward ();
	clause (false);
      }
    }
  }
  lits.shrink ();
  return res;
}

void Solver::checkgate () {
#ifndef NDEBUG
  for (int i = 0; i < posgate; i++) assert (gate[i]->contains (gatepivot));
  for (int i = posgate; i < gate; i++) assert (gate[i]->contains (1^gatepivot));
  for (int i = 0; i < posgate; i++)
    for (int j = posgate; j < gate; j++)
      assert (!resolve (gate[i], gatepivot, gate[j], true));
#endif
}

bool Solver::trelim (int idx) {
  assert (!conflict);
  assert (queue == trail);
  assert (!vars[idx].eliminated);
  int lit = 2 * idx;
  if (vals[lit] || repr[lit]) return false;
  LOG ("trelim " << lit << " bound " << elms[idx].heat);
  int gain = orgs[lit] + orgs[lit^1];
  if (gate) {
    LOG ("actually trelim gate for " << gatepivot);
    assert (gatepivot/2 == lit/2);
    int piv = gatepivot;
    for (int i = 0; !conflict && i < posgate && gain >= 0; i++)
      for (unsigned j = 0; !conflict && gain >= 0 && j < orgs[piv^1]; j++)
	if (resolve (gate[i], piv, orgs[piv^1][j], true)) gain--;
    for (int i = posgate; !conflict && i < gate && gain >= 0; i++)
      for (unsigned j = 0; !conflict && gain >= 0 && j < orgs[piv]; j++)
	if (resolve (orgs[piv][j], piv, gate[i], true)) gain--;
  } else
    for (unsigned i = 0; !conflict && gain >= 0 && i < orgs[lit]; i++)
      for (unsigned j = 0; !conflict && gain >= 0 && j < orgs[lit^1]; j++)
	if (resolve (orgs[lit][i], lit, orgs[lit^1][j], true)) gain--;

  if (conflict) return false;
  LOG ("approximated gain " << gain);
  return gain >= 0;
}

void Solver::elim (int idx) {
  int lit = 2 * idx;
  assert (!conflict);
  assert (!units);
  assert (queue == trail);
  assert (!vars[idx].eliminated);
  assert (!vals[lit]);
  assert (!repr[lit]);
  LOG ("elim " << lit);
  assert (!vals[lit] && !repr[lit]);
  {
    elits.push (mem, 0);
    int slit;
    Cls ** begin, ** end;
    LOG ("elit 0");
    if (gate) {
      slit = gatepivot;
      int pos = posgate;
      int neg = gate - pos;
      begin = gate.begin ();
      if (pos < neg) end = begin + posgate;
      else begin += posgate, end = gate.end (), slit ^= 1;
    } else {
      slit = lit;
      int pos = orgs[lit];
      int neg = orgs[lit^1];
      if (pos < neg) begin = orgs[lit].begin (), end = orgs[lit].end ();
      else begin = orgs[1^lit].begin (), end = orgs[1^lit].end (), slit ^= 1;
    }
    for (Cls ** p = begin; p < end; p++) {
      elits.push (mem, 0);
      LOG ("elit 0");
      Cls * c = *p;
      int other;
      for (int * p = c->lits; (other = *p); p++) {
	if (other == (slit)) continue;
	assert (other != (1^slit));
	elits.push (mem, other);
	LOG ("elit " << other);
      }
    }
    elits.push (mem, slit);
    LOG ("elit " << (slit));
  }

  vars[idx].eliminated = true;
  stats.vars.elim++;

  for (int sign = 0; sign <= 1; sign++)
    for (unsigned i = 0; i < orgs[lit^sign]; i++) {
      Cls *  c = orgs[lit^sign][i];
      c->dirty = true;
    }

  int gatecount = gate;
  if (gatecount) {
    LOG ("actually elim gate for " << gatepivot);
    assert (gatepivot/2 == lit/2);
    int piv = gatepivot;
    for (int i = 0; !conflict && i < posgate; i++)
      for (unsigned j = 0; !conflict && j < orgs[piv^1]; j++)
	resolve (gate[i], piv, orgs[piv^1][j], false);
    for (int i = posgate; !conflict && i < gate; i++)
      for (unsigned j = 0; !conflict && j < orgs[piv]; j++)
	resolve (orgs[piv][j], piv, gate[i], false);
  } else {
    for (unsigned i = 0; !conflict && i < orgs[lit]; i++)
      for (unsigned j = 0; !conflict && j < orgs[lit^1]; j++)
	resolve (orgs[lit][i], lit, orgs[lit^1][j], false);
  }

  for (int sign = 0; sign <= 1; sign++)
    for (unsigned i = 0; i < orgs[lit^sign]; i++)
      orgs[lit^sign][i]->dirty = false;

  assert (gate == gatecount);
  cleangate (), cleantrash ();

  for (int sign = 0; sign <= 1; sign++) {
    int slit = lit^sign;
    while (orgs[slit]) recycle (orgs[slit][0]);
    stats.collected += orgs[slit].bytes ();
    orgs[slit].release (mem);
  }
}

inline long long Solver::clauses () const {
  return stats.clauses.original + stats.clauses.learned + stats.clauses.binary;
}

inline bool Solver::hasgate (int idx) {
  assert (0 < idx && idx <= size);
  assert (!gate);
  int lit = 2*idx;
  if (andgate (lit)) return true;
  if (andgate (1^lit)) return true;
  if (xorgate (lit)) return true;
  if (itegate (lit)) return true;
  return false;
}

void Solver::fwbw () {
  int l = 5000;
  limit.budget.fw.sub = 10 * l;
  limit.budget.bw.sub = 5 * l;
  limit.budget.fw.str = 4 * l;
  limit.budget.bw.str = 2 * l;
  for (int round = 0; !conflict && round <= 1; round++) {
    Cls * p, * prev;
    prev = (round ? binary : original).head;
    while ((p = prev)) {
      prev = p->prev;
      if (p->lnd) continue;
      if (prev) prev->dirty = true;
      assert (!lits);
      for (const int * r = p->lits; *r; r++) 
	lits.push (mem, *r);
      p->dirty = true;
      int oldsize = lits;
      bool fwd = forward ();
      p->dirty = false;
      if (fwd || (oldsize > lits)) {
	recycle (p);
	backward ();
	clause (false);
      } else { 
	assert (oldsize == lits);
      }
      cleantrash ();
      lits.shrink ();
      if (prev) prev->dirty = false;
      if (!bcp ()) break;;
    }
  }
}

void Solver::elim () {
  if (!eliminating ()) return;
  if (!bcp ()) return;
  stats.elim.phases++;
  report (1, 'e');
  elimode = true;
  checkclean ();
  disconnect ();
  initorgs ();
  initfwds ();
  initsigs ();
  connect (binary, true);
  connect (original, true);
  if (schedule.elim) {
    for (int idx = 1; idx <= size; idx++) {
      Rnk * e = elms + idx;
      if (schedule.elim.contains (e)) touch (2*idx);
    }
  } else {
    for (int idx = 1; idx <= size; idx++) touch (2*idx);
    fwbw ();
  }
  long long bound = opts.elimint;
  if (stats.elim.rounds == 0) bound *= 2;
  bound += stats.elim.resolutions;
  limit.budget.bw.sub = limit.budget.bw.sub = bound;
  limit.budget.fw.str = limit.budget.fw.str = bound;
  while (!conflict && schedule.elim && stats.elim.resolutions <= bound) {
    Rnk * e = schedule.elim.pop ();
    int idx = e - elms, lit = 2 * idx;
    if (vals[lit]) continue;
    if (hasgate (idx)) assert (gatestats), checkgate ();
    bool eliminate = trelim (idx);
    if (!bcp ()) break;
    if (!eliminate || vals[lit]) { cleangate (); continue; }
    if (gatestats)
      gatestats->count += 1, gatestats->len += gatelen, stats.vars.subst++;
    elim (idx);
    if (!bcp ()) break;
    bound += 100;
  }

  if (conflict) return;

  assert (!gate);
  assert (!trash);

  for (int i = 0; i <= 2; i++) {
    Cls * p = 0;
    if (i == 0) p = binary.head;
    if (i == 1) p = original.head;
    if (i == 2) p = learned.head;
    while (p) {
      assert (!p->locked); assert (!p->garbage);
      Cls * prev = p->prev;
      int other;
      for (int * q = p->lits; (other = *q); q++)
	if (vars[*q/2].eliminated) break;
      if (other) p->garbage = true;
      p = prev;
    }
  }
  delorgs ();
  delfwds ();
  delsigs ();
  gc ();
  checkeliminated ();
  elimode = false;

  if (!schedule.elim) {
    stats.elim.rounds++;
    limit.fixed.elim = stats.vars.fixed + stats.vars.merged;
  }
  limit.props.elim = stats.props + opts.elimprd * (long long)opts.elimint;
}

void Solver::jwh () {
  memset (jwhs, 0, 2 * (size + 1) * sizeof *jwhs);
  for (Cls * p = original.head; p; p = p->prev) jwh (p);
  for (Cls * p = binary.head; p; p = p->prev) jwh (p);
  for (Cls * p = learned.head; p; p = p->prev) jwh (p);
}

void Solver::initreduce () {
  int l = stats.clauses.original;
  if (l > opts.maxlimit) l = opts.maxlimit;
  if (l < opts.minlimit) l = opts.minlimit;
  limit.reduce.learned = limit.enlarge = l;
  report (0, 'l');
}

void Solver::enlarge () {
  stats.enlarged++;
  limit.reduce.learned *= 11; limit.reduce.learned /= 10;
  if (limit.reduce.learned > opts.maxlimit) {
    limit.reduce.learned = opts.maxlimit;
    limit.enlarge = INT_MAX;
  }
  report (0, '+');
  limit.enlarge = 5 * (stats.conflicts / 4);
}

void Solver::shrink (int old) {
  stats.shrunken++;
  int now = stats.clauses.original;
  if (!now) return;
  old /= 100, now /= 100;
  if (old <= now) return;
  assert (old);
  int red = (100 * (old - now)) / old;
  assert (red <= 100);
  if (!red) return;

  if (limit.enlarge >= opts.maxlimit) limit.enlarge = opts.maxlimit;
  limit.enlarge = ((100*limit.enlarge - red*limit.enlarge)+99)/100;
  if (limit.enlarge < opts.minlimit) limit.enlarge = opts.minlimit;

  limit.reduce.learned = 
    ((100*limit.reduce.learned - red*limit.reduce.learned)+99)/100;
  if (limit.reduce.learned < opts.minlimit)
    limit.reduce.learned = opts.minlimit;

  report (0, '-');
}

void Solver::simplify () {
  assert (!conflict);
  assert (queue == trail);
  stats.simps++;
  int old = stats.clauses.original;
  undo (0);
  decompose ();
  if (!bcp ()) return;
  gc ();
  elim ();
  jwh ();
  limit.props.simp = stats.props + opts.simprd * clauses ();
  limit.simp = stats.vars.fixed + stats.impls + stats.vars.merged;
  limit.fixed.simp = stats.vars.fixed;
  report (0, 's');
  if (stats.simps == 1) initreduce ();
  else shrink (old);
}

void Solver::flush () {
  assert (units);
  undo (0);
  while (units && !conflict) unit (units.pop ());
}

void Solver::initrestart () {
  limit.restart.conflicts = opts.restartint * luby (limit.restart.lcnt = 1);
  limit.restart.conflicts += stats.conflicts;
}

void Solver::initbias () {
  jwh ();
  limit.rebias.conflicts = opts.rebiasint * luby (limit.rebias.lcnt = 1);
  limit.rebias.conflicts += stats.conflicts;
}

void Solver::iteration () {
  assert (!level);
  assert (iterating);
  assert (limit.fixed.iter < stats.vars.fixed);
  stats.iter++;
  initrestart ();
  int old = stats.clauses.original;
  while (limit.fixed.iter < stats.vars.fixed) {
    int lit = trail[limit.fixed.iter++];
    recycle (lit);
  }
  iterating = false;
  report (1, 'i');
  shrink (old);
}

inline bool Solver::reducing () const {
  int learned_not_locked = stats.clauses.learned;
  learned_not_locked -= stats.clauses.locked;
  return 2 * learned_not_locked > 3 * limit.reduce.learned;
}

inline bool Solver::eliminating () const {
  if (!opts.elim) return false;
  if (!stats.elim.phases) return true;
  if (schedule.elim) return true;
  if (stats.props <= limit.props.elim) return false;
  return stats.vars.fixed + stats.vars.merged > limit.fixed.elim;
}

inline bool Solver::simplifying () const {
  if (!stats.simps) return true;
  if (limit.fixed.simp + 10000 < stats.vars.fixed) return true;
  if (stats.props < limit.props.simp) return false;
  return limit.simp < stats.vars.fixed + stats.vars.merged + stats.impls;
}

inline bool Solver::restarting () const {
  return level >= 2 && limit.restart.conflicts <= stats.conflicts;
}

inline bool Solver::rebiasing () const {
  return limit.rebias.conflicts <= stats.conflicts;
}

inline bool Solver::probing () const {
  if (!opts.probe) return false;
  if (stats.props < limit.props.probe) return false;
  if (!level) return true;
  if (schedule.probe) return true;
  return limit.fixed.probe < stats.vars.fixed;
}

inline bool Solver::enlarging () const {
  if (limit.enlarge >= opts.maxlimit) return false;
  return stats.conflicts >= limit.enlarge;
}

inline bool Solver::exhausted () const {
  return stats.decisions >= limit.decisions;
}

int Solver::search () {
  for (;;)
    if (!bcp ()) { if (!analyze ()) return -1; }
    else if (iterating) iteration ();
    else if (units) flush ();
    else if (probing ()) probe ();
    else if (simplifying ()) simplify  ();
    else if (rebiasing ()) rebias ();
    else if (restarting ()) restart ();
    else if (reducing ()) reduce ();
    else if (enlarging ()) enlarge ();
    else if (exhausted ()) return 0;
    else if (!decide ()) return 1;
}

void Solver::initlimit (int decision_limit) {
  memset (&limit, 0, sizeof limit);
  limit.decisions = decision_limit;
  rng.init (opts.seed);
}

void Solver::init (int decision_limit) {
  assert (opts.fixed);
  initlimit (decision_limit);
  initbias ();
  initrestart ();
  report (0, '*');
}

int Solver::solve (int decision_limit) {
  init (decision_limit);
  int res = search ();
  report (0, res < 0 ? '0' : (res ? '1' : '?'));
  printf ("c\n");
  fflush (stdout);
  return res;
}

double Stats::now () {
  struct rusage u;
  if (getrusage (RUSAGE_SELF, &u))
    return 0;
  double res = u.ru_utime.tv_sec + 1e-6 * u.ru_utime.tv_usec;
  res += u.ru_stime.tv_sec + 1e-6 * u.ru_stime.tv_usec;
  return res;
}

double Stats::seconds () {
  double t = now (), delta = t - entered;
  delta = (delta < 0) ? 0 : delta;
  time += delta;
  entered = t;
  return time;
}

Stats::Stats () {
  memset (this, 0, sizeof *this);
  entered = now ();
}

Limit::Limit () {
  memset (this, 0, sizeof *this);
  budget.fw.sub = budget.bw.sub = budget.bw.str = budget.bw.str = 10000;
}

#include <signal.h>

static Solver * solverptr;
static bool catchedsig;

static void (*sig_int_handler)(int);
static void (*sig_segv_handler)(int);
static void (*sig_abrt_handler)(int);
static void (*sig_term_handler)(int);

static void
resetsighandlers (void) {
  (void) signal (SIGINT, sig_int_handler);
  (void) signal (SIGSEGV, sig_segv_handler);
  (void) signal (SIGABRT, sig_abrt_handler);
  (void) signal (SIGTERM, sig_term_handler);
}

static void caughtsigmsg (int sig) {
  printf ("c\nc *** CAUGHT SIGNAL %d ***\nc\n", sig);
  fflush (stdout);
}

static void catchsig (int sig) {
  if (!catchedsig) {
    catchedsig = true;
    caughtsigmsg (sig);
    solverptr->prstats ();
    caughtsigmsg (sig);
  }

  resetsighandlers ();
  raise (sig);
}

static void
setsighandlers (void) {
  sig_int_handler = signal (SIGINT, catchsig);
  sig_segv_handler = signal (SIGSEGV, catchsig);
  sig_abrt_handler = signal (SIGABRT, catchsig);
  sig_term_handler = signal (SIGTERM, catchsig);
}

static const char * usage =
"usage: precosat [-h|-v|-n][-l <lim>][--[no-]<opt>[=<val>]] [<dimacs>]\n"
"\n"
"  -h             this command line summary\n"
"  -v             increase verbosity\n"
"  -n             do not print witness\n"
"  -l  <lim>      set decision limit\n"
"  <dimacs>       dimacs input file (default stdin)\n"
"\n"
"  --<opt>        set internal option <opt> to 1\n"
"  --no-<opt>     set internal option <opt> to 0\n"
"  --<opt>=<val>  set internal option <opt> to integer value <val>\n"
;

int main (int argc, char ** argv) {
  bool fclosefile = false, pclosefile = false, nowitness = false;
  int verbose = 0, decision_limit = INT_MAX;
  const char * name = 0;
  FILE * file = stdin;
  
  FILE * SIEGE_TMP_FILE = NULL;
   FILE * SIEGE_TMP_FILE2 = fopen("siege.status", "w");

  for (int i = 1; i < argc; i++) {
    if (!strcmp (argv[i], "-h")) {
      fputs (usage, stdout);
      exit (0);
    }
    if (!strcmp (argv[i], "-v")) { verbose++; continue; }
    if (!strcmp (argv[i], "--verbose")) { verbose++; continue; }
    if (!strcmp (argv[i], "--verbose=1")) { verbose++; continue; }
    if (!strcmp (argv[i], "-n")) { nowitness = true; continue; }
    if (!strcmp  (argv[i], "-l")) {
      if (i + 1 == argc) {
	fprintf (stderr, "*** precosat: argument to '-l' missing\n");
	exit (1);
      }
      decision_limit = atoi (argv[++i]);
      continue;
    }
    if (argv[i][0] == '-' && argv[i][1] == '-') continue;
    if (argv[i][0] == '-' || name) {
      
      /*fprintf (stderr, "*** R.H. precosat: invalid usage (try '-h')\n");
      exit (1);
      */
    }

  }

    name = argv[1];
    if (hasgzsuffix (name)) {
      char * cmd = (char*) allocate (strlen(name) + 100);
      sprintf (cmd, "gunzip -c %s 2>/dev/null", name);
      if ((file = popen (cmd, "r")))
        pclosefile = true;
      free (cmd);
    } else if ((file = fopen (name, "r")))
      fclosefile = true;
  
  if (!name) name = "<stdin>";
  if (!file) {
    fprintf (stderr, "*** precosat: can not read '%s'\n", name);
    exit (1);
  }
	//precosat_banner ();
  printf ("c\nc reading %s\n", name);
  fflush (stdout);
  int ch;
  while ((ch = getc (file)) == 'c')
    while ((ch = getc (file)) != '\n' && ch != EOF)
      ;
  if (ch == EOF)
    goto INVALID_HEADER;
  ungetc (ch, file);
  int m, n;
  if (fscanf (file, "p cnf %d %d\n", &m, &n) != 2) {
INVALID_HEADER:
    fprintf (stderr, "*** precosat: invalid header\n");
    exit (1);
  }
  printf ("c found header 'p cnf %d %d'\n", m, n);
  fflush (stdout);
  Solver solver (m);
  for (int i = 1; i < argc; i++) {
    bool ok = true;
    if (!strcmp (argv[i], "-v")) solver.set ("verbose", 1);
    else if (argv[i][0] == '-' && argv[i][1] == '-') {
      if (argv[i][2] == 'n' && argv[i][3] == 'o' && argv[i][4] == '-') {
	if (!solver.set (argv[i] + 5, 0)) ok = false;
      } else {
	const char * arg = argv[i] + 2, * p;
	for (p = arg; *p && *p != '='; p++)
	  ;
	if (*p) {
	  assert (*p == '=');
	  char * opt = strndup (arg, p - arg);
	  if (!solver.set (opt, atoi (p + 1))) ok = false;
	  free (opt);
	} else if (!solver.set (argv[i] + 2, 1)) ok = false;
      }
    }
    if (!ok) {
      fprintf (stderr, "*** precosat: invalid option '%s'\n", argv[i]);
      exit (1);
    }
  }
  solver.fxopts ();
  solverptr = &solver;
  setsighandlers ();
  int lit, sign;
  int res = 0;

  ch = getc (file);

NEXT:

  if (ch == 'c') {
      while ((ch = getc (file)) != '\n')
	if (ch == EOF) {
	  fprintf (stderr, "*** precosat: EOF in comment\n");
	  resetsighandlers ();
	  exit (1);
	}
      goto NEXT;
  }

  if (ch == ' ' || ch == '\n') {
    ch = getc (file);
    goto NEXT;
  }

  if (ch == EOF) goto DONE;

  if (!n) {
    fprintf (stderr, "*** precosat: too many clauses\n");
    res = 1;
    goto EXIT;
  }

  if ((sign = (ch == '-'))) ch = getc (file);

  if (!isdigit (ch)) {
    fprintf (stderr, "*** precosat: expected digit\n");
    res = 1;
    goto EXIT;
  }

  lit = (ch - '0');
  while (isdigit (ch = getc (file))) lit = 10 * lit + (ch - '0');

  if (lit > m) {
    fprintf (stderr, "*** precosat: variables exceeded\n");
    res = 1;
    goto EXIT;
  }

  lit = 2 * lit + sign;
  solver.add (lit);
  if (!lit) n--;

  goto NEXT;

DONE:

  if (n) {
    fprintf (stderr, "*** precosat: clauses missing\n");
    res = 1;
    goto EXIT;
  }

  if (pclosefile) pclose (file);
  if (fclosefile) fclose (file);

  
  printf ("c finished parsing\n");
  printf ("c\n"); solver.propts (); printf ("c\n");
  printf ("c starting search after %.1f seconds\n", solver.seconds ());
  if (decision_limit == INT_MAX) printf ("c no decision limit\n");
  else printf ("c search limited to %d decisions\n", decision_limit);
  fflush (stdout);

  res = solver.solve (decision_limit);

  if (argc>2)
   SIEGE_TMP_FILE = fopen(argv[2], "w");
  if (res > 0) {
    if (!solver.satisfied ()) {
      printf ("c ERROR\n");
      abort ();
    }
    printf ("s SATISFIABLE\n");
    if (SIEGE_TMP_FILE) fprintf(SIEGE_TMP_FILE, "SATISFIABLE\n\n");
    fprintf(SIEGE_TMP_FILE2, "SATISFIABLE\n\n");
    if (!nowitness) {
      fflush (stdout);
      if (m) fputs ("v", stdout);
      for (int i = 1; i <= m; i++) {
        /*
        if (i % 8) fputc (' ', stdout);
        else fputs ("\nv ", stdout); */
        //printf ("%d ", (solver.val (2 * i) < 0) ? -i : i);
        if (SIEGE_TMP_FILE && (solver.val (2 * i) >= 0) )  
            fprintf(SIEGE_TMP_FILE, "%d ",  i );        
        fprintf(SIEGE_TMP_FILE2, "%d ", (solver.val (2 * i) < 0) ? -i : i);        
      }
      if (m) fputc ('\n', stdout);
      fputs ("v 0\n", stdout);
    }
    res = 10;
  } else if (res < 0) {
    assert (res < 0);
    printf ("s UNSATISFIABLE\n");
    if (SIEGE_TMP_FILE)  fprintf(SIEGE_TMP_FILE, "UNSATISFIABLE\n\n");    
    fprintf(SIEGE_TMP_FILE2, "UNSATISFIABLE\n\n");    
    res = 20;
  } else
    printf ("s UNKNOWN\n");
  fflush (stdout);

EXIT:
  resetsighandlers ();
  printf ("c\n");
  solver.prstats ();

  return res;
}

template<class T> void Stack<T>::enlarge (Mem & m) {
  assert (t == e);
  m -= bytes ();
  int o = size ();
  int s = o ? 2 * o : 1;
  size_t b = s * sizeof (T);
  a = (T*) reallocate (a, b);
  t = a + o;
  e = a + s;
  m += b;
}

template<class T> void Crack<T>::enlarge (Mem & m) {
  size_t b;
  if (a) { b = bytes (); a = (T*) reallocate (a, 2 * b); s++; }
  else { b = sizeof (T); a = (T*) reallocate (a, b); s = 0; }
  m += b;
}

void PrecoSat::die (const char * msg, ...) {
  va_list ap;
  fputs ("c\nc ", stdout);
  va_start (ap, msg);
  vprintf (msg, ap);
  va_end (ap);
  fputs ("\nc\n", stdout);
  fflush (stdout);
  abort ();
}

static void PrecoSat::msg (const char * msg, ...) {
  va_list ap;
  fputs ("c ", stdout);
  va_start (ap, msg);
  vprintf (msg, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
}

void Solver::print (const char * type, Anchor<Cls>& anchor) {
  for (Cls * p = anchor.tail; p; p = p->next) {
    printf ("c PRINT %s clause", type);
    for (int * q = p->lits; *q; q++)
      printf (" %d", *q);
    fputc ('\n', stdout);
  }
}

void Solver::print (void) {
  print ("binary", binary);
  print ("original", original);
  print ("learned", learned);
}

#ifndef NLOGPRECO
void Solver::printgate (void) {
  for (Cls ** p = gate.begin(); p < gate.end (); p++) {
    printf ("c LOG gate clause");
    Cls * c = *p;
    for (const int * q = c->lits; *q; q++)
      printf (" %d", *q);
    printf ("\n");
  }
}
#endif
