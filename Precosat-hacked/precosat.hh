// Copyright (c) 2009, Armin Biere, JKU, Linz, Austria.  All rights reserved.

#ifndef PrecoSat_hh_INCLUDED
#define PrecoSat_hh_INCLUDED

#include <cassert>
#include <cstdlib>
#include <climits>
#include <cstdarg>
#include <cstring>

namespace PrecoSat {

void die (const char *, ...);

struct Mem {
  size_t cur, max;
  Mem () : cur (0), max (0) { }
  void operator += (size_t b) { if ((cur += b) > max) max = cur; }
  void operator -= (size_t b) { assert (cur >= b); cur -= b; }
  operator size_t () const { return cur; }
};

template<class T> struct Crack {
  T * a;
  static const unsigned LDMAXS = (sizeof(void*)==4 ?  5 : 32);
  static const unsigned LDMAXN = (sizeof(void*)==4 ? 27 : 32);
  unsigned s:LDMAXS;
  unsigned n:LDMAXN;
  static const unsigned MAXS = (sizeof(void*)==4 ? ((1<< 5)-1) : ~0u);
  static const unsigned MAXN = (sizeof(void*)==4 ? ((1<<27)-1) : ~0u);
  unsigned size () const { return a ? (1<<s) : 0; }
  void enlarge (Mem & m);
  Crack () : a (0), n (0) { }
  ~Crack () { free (a); }
  operator unsigned () const { return n; }
  size_t bytes () { return size () * sizeof (T); }
  void push (Mem & m, const T & e) {
     if (n == MAXN) die ("maximum compact stack size exceeded");
     if (n == size ()) enlarge (m); 
     a[n++] = e; 
  }
  T & operator [] (unsigned i) { assert (i < n); return a[i]; }
  void shrink (unsigned m = 0) { assert (m <= n); n = m; }
  void release (Mem & m) { m -= bytes (); n = s = 0; free (a); a = 0; }
  void remove (const T & d) { // TODO start from back and use ptrs
    unsigned i;
    for (i = 0; !(a[i] == d); i++) assert (i+1 < n);
    while (++i < n) a[i-1] = a[i];
    n--;
  }
  T * begin () { return a; }
  T * end () { return a + n; }
  const T * begin () const { return a; }
  const T * end () const { return a + n; }
  bool contains (const T & d) const {
    for (const T * p = begin (); p < end (); p++) if (*p == d) return true;
    return false;
  }
};

template<class T> class Stack {
  T * a, * t, * e;
  int size () const { return e - a; }
  void enlarge (Mem & m);
public:
  Stack () : a (0), t (0), e (0) { }
  ~Stack () { free (a); }
  operator int () const { return t - a; }
  void push (Mem & m, const T & d) { if (t == e) enlarge (m); *t++ = d; }
  const T & pop () { assert (a < t); return *--t; }
  const T & top () { assert (a < t); return t[-1]; }
  T & operator [] (int i) { assert (i < *this); return a[i]; }
  void shrink (int m = 0) { assert (m <= *this); t = a + m; }
  void shrink (T * nt) { assert (a <= nt); assert (nt <= t); t = nt; }
  void release (Mem & m) { m -= bytes (); free (a); a = t = e = 0; }
  const T * begin () const { return a; }
  const T * end () const { return t; }
  T * begin () { return a; }
  T * end () { return t; }
  size_t bytes () { return size () * sizeof (T); }
  void remove (const T & d) { // TODO start from back
    T * p = a;
    while (!(*p == d)) { p++; assert (p < t); }
    while (++p < t) p[-1] = *p;
    t--;
  }
};

template<class T, class L> class Heap {
  Stack<T *> stack;
  static int left (int p) { return 2 * p + 1; }
  static int right (int p) { return 2 * p + 2; }
  static int parent (int c) { return (c - 1) / 2; }
public:
  void release (Mem & mem) { stack.release (mem); }
  operator int () const { return stack; }
  T * operator [] (int i) { return stack[i]; }
  bool contains (T * e) const { return e->pos >= 0; }
  void up (T * e) { 
    int epos = e->pos;
    while (epos > 0) {
      int ppos = parent (epos);
      T * p = stack [ppos];
      if (L (p, e)) break;
      stack [epos] = p, stack [ppos] = e;
      p->pos = epos, epos = ppos;
    }
    e->pos = epos;
  }
  void down (T * e) {
    assert (contains (e));
    int epos = e->pos;
    for (;;) {
      int cpos = left (epos);
      if (cpos >= stack) break;
      T * c = stack [cpos], * o;
      int opos = right (epos);
      if (L (e, c)) {
	if (opos >= stack) break;
	o = stack [opos];
	if (L (e, o)) break;
	cpos = opos, c = o;
      } else if (opos < stack) {
	o = stack [opos];
	if (!L (c, o)) { cpos = opos; c = o; }
      }
      stack [cpos] = e, stack [epos] = c;
      c->pos = epos, epos = cpos;
    }
    e->pos = epos;
  }
  T * max () { assert (stack); return stack[0]; }
  void push (Mem & mem, T * e) {
    assert (!contains (e));
    e->pos = stack;
    stack.push (mem, e);
    up (e);
  }
  T * pop () {
    int size = stack;
    assert (size);
    T * res = stack[0];
    assert (!res->pos);
    if (--size) {
      T * other = stack[size];
      assert (other->pos == size);
      stack[0] = other, other->pos = 0;
      stack.shrink (size);
      down (other);
    } else
      stack.shrink ();
    res->pos = -1;
    return res;
  }
  void shrink () {
    for (T ** p = stack.begin (); p < stack.end (); p++) {
      T * e = *p; assert (e->pos >= 0); e->pos = 0;
    }
    stack.shrink ();
  }
};

template<class A, class E> 
void dequeue (A & anchor, E * e) {
  if (anchor.head == e) { assert (!e->next); anchor.head = e->prev; }
  else { assert (e->next); e->next->prev = e->prev; }
  if (anchor.tail == e) { assert (!e->prev); anchor.tail = e->next; }
  else { assert (e->prev); e->prev->next = e->next; }
  e->prev = e->next = 0;
}

template<class A, class E> 
bool connected (A & anchor, E * e) {
  if (e->prev) return true;
  if (e->next) return true;
  return anchor.head == e;
}

template<class A, class E>
void push (A & anchor, E * e) {
  e->next = 0;
  if (anchor.head) anchor.head->next = e;
  e->prev = anchor.head;
  anchor.head = e; 
  if (!anchor.tail) anchor.tail = e;
}

template<class A, class E> 
void mtf (A & a, E * e) { dequeue (a, e); push (a, e); }

struct Cls;

template<class E> struct Anchor { 
  E * head, * tail;
  Anchor () : head (0), tail (0) { }
};

struct Rnk { int heat; int pos; };

struct Hotter {
  Rnk * a, * b;
  Hotter (Rnk * r, Rnk * s) : a (r), b (s) { }
  operator bool () const { 
    if (a->heat > b->heat) return true;
    if (a->heat < b->heat) return false;
    return a < b;
  }
};

typedef signed char Val;

struct Var {
  Val phase:2, mark:2;
  bool onstack:1, removable:1, unremovable:1, binary:1, eliminated:1;
  static const int LDMAXL = (sizeof(void*)==8) ? 32 : 23;
  static const int MAXL = (sizeof(void*)==8) ? INT_MAX : (1<<22)-1;
  int tlevel : LDMAXL;
  int dlevel, dominator;
  union { Cls * cls; int lit; } reason;
};

enum Type { ORIGINAL, BINARY, LEARNED };

class Hash {
  Cls ** table;
  static Cls * REMOVED;
  unsigned size, count, removed;
  static bool match (Cls*,int,int,int);
  unsigned hash (int,int,int);
  unsigned delta (int,int,int);
  Cls ** find (int,int,int,bool overwrite=false);
  void enlarge (Mem &);
public:
  long long hashed, collisions;
  Hash () { memset (this, 0, sizeof *this); }
  Cls * contains (int,int,int last = 0);
  bool cntnd (int,int,int last = 0);
  void insert (Mem &, Cls *);
  void remove (Cls *);
  void release (Mem &);
};

struct Cls {
  static const int LDMAXSZ = 24, MAXSZ = (1<<LDMAXSZ)-1;
  bool locked:1,lnd:1,garbage:1,binary:1,trash:1,dirty:1,gate:1,hashed:1;
  unsigned size : LDMAXSZ;
  unsigned sig;
  Cls * prev, * next;
  int lits[4];
  static size_t bytes (int n);
  size_t bytes () const;
  Cls (int l0 = 0, int l1 = 0, int l2 = 0) 
    { lits[0] = l0; lits[1] = l1; lits[2] = l2; lits[3] = 0; }
  int minlit () const;
  bool contains (int) const;
};

struct Frame {
  bool pulled : 1;
  int tlevel : 31;
  Frame (int t) : pulled (false), tlevel (t) { }
};

struct GateStats { int count, len; };

struct Stats {
  struct { int fixed, equiv, elim, subst, merged; } vars;
  struct { int original, binary, learned, locked; } clauses;
  struct { long long added; int bssrs, ssrs; } lits;
  struct { long long deleted, strong; int depth; } mins;
  int conflicts, decisions, impls, random, enlarged, shrunken, rescored, iter;
  struct { int count, skipped, maxlubydelta; } restart;
  struct { int count, level1, probing; } doms;
  struct { GateStats nots, ites, ands, xors; } subst;
  struct { int fw, bw, doms; } subs;
  struct { int fw, bw; } str;
  struct { int units, merged; } hash;
  struct { int count, maxlubydelta; } rebias;
  struct { int variables, phases, rounds, failed, lifted, merged; } probe;
  struct { int nontriv, fixed, merged; } sccs;
  struct { long long resolutions; int phases, rounds; } elim;
  struct { struct { struct { long long srch,hits; } l1,l2;} fw,bw;} sigs;
  long long props, visits, ternaryvisits, sumheight, collected;
  int simps, reductions, reports, maxdepth;
  double entered, time;
  static double now ();
  double seconds ();
  double height ();
  Stats ();
};

struct Limit {
  int enlarge, decisions, simp;
  struct { int conflicts, lcnt; } restart;
  struct { int conflicts, lcnt; } rebias;
  struct { int learned, delay, logd; } reduce;
  struct { int iter, reduce, probe, elim, simp; } fixed;
  struct { long long simp, probe, elim; } props;
  struct { struct { int sub, str; } fw; struct { int sub, str; } bw; } budget;
  Limit ();
};

struct Opt {
  const char * name;
  int * valptr, min, max;
  Opt (const char * n, int v, int * vp, int mi, int ma);
};

struct Opts {
  int verbose;
  int dominate;
  int merge;
  int hash;
  int simprd;
  int probe, probeprd, probeint;
  int decompose;
  int elim, elimprd, elimint;
  int subst, ands, xors, ites;
  int restart, restartint;
  int rebias, rebiasint;
  int minlimit, maxlimit;
  int random, spread, seed;
  int order;
  enum MinMode { NONE=0, LOCAL=1, RECUR=2, STRONG=3 };
  int minimize, maxdepth, strength;
  int check;
  int skip;
  bool fixed;
  Stack<Opt> opts;
  Opts () : fixed (false) { }
  bool set (const char *, int);
  void add (Mem &, const char * name, int, int *, int, int);
  void print () const;
};

struct SCC { unsigned idx, min : 31; bool done : 1; };

class RNG {
  unsigned state;
public:
  RNG () : state (0) { }
  unsigned next ();
  bool oneoutof (unsigned);
  void init (unsigned seed) { state = seed; }
};

struct Occ {
  int blit;
  Cls * cls;
  Occ (int b, Cls * c) : blit (b), cls (c) { }
  bool operator == (const Occ & o) const 
    { return cls ? o.cls == cls : o.blit == blit; }
};

typedef Crack<Occ> Occs;
typedef Crack<Cls*> Orgs;
typedef Crack<Cls*> Fwds;

class Solver {
  Val * vals;
  Var * vars;
  int * jwhs, * repr, * iirfs;
  Occs * occs;
  Orgs * orgs;
  Fwds * fwds;
  unsigned * bwsigs, * fwsigs;
  Rnk * rnks, * prbs, * elms;
  Hash hash;
  struct { Heap<Rnk,Hotter> decide, probe, elim; } schedule;
  int size, queue, queue2, level, jumplevel, open;
  Stack<int> trail, lits, units, levels, saved, elits, check;
  Stack<Frame> frames;
  Stack<Var *> seen;
  Stack<Cls *> pulled, trash, gate;
  Cls * conflict, empty, dummy;
  Anchor<Cls> original, binary, learned;
  int hinc, agility, spread, posgate, gatepivot, gatelen;
  GateStats * gatestats;
  bool measure, iterating, elimode, extending;
  RNG rng;
  Stats stats;
  Limit limit;
  Opts opts;
  Mem mem;

  Rnk * prb (const Rnk *);
  Rnk * rnk (const Var *);
  Var * var (const Rnk *);

  int & iirf (Var *, int);

  Val fixed (int lit) const;

  void initfwds ();
  void initsigs ();
  void initorgs ();
  void delorgs ();
  void delfwds ();
  void delsigs ();
  void deliirfs ();
  void delclauses (Anchor<Cls> &);
  Anchor<Cls> & anchor (Cls *);

  void initreduce ();
  void initbias ();
  void initrestart ();
  void initlimit (int);
  void init (int);

  long long clauses () const;
  int recyclelimit () const;
  bool recycling () const;
  bool reducing () const;
  bool eliminating () const;
  bool simplifying () const;
  bool restarting () const;
  bool rebiasing () const;
  bool probing () const;
  bool enlarging () const;
  bool exhausted () const;

  void marklits (Cls *);
  void marklits ();
  void unmarklits ();
  void unmarklits (Cls *);
  bool forward ();
  void backward ();
  void backward (Cls *);
  Cls * clause (bool learned);
  unsigned litsig ();
  int redundant (Cls *);
  void recycle (Cls *);
  void recycle (int);
  void setsig (Cls *);
  void connect (Cls *);
  void connect (Anchor<Cls>&, bool orgonly = false);
  void disconnect (Cls *);
  void disconnect ();
  void collect (Cls *);
  int bwstr (unsigned sig, Cls *);
  int fwstr (unsigned sig, Cls *);
  void remove (int lit, Cls *);
  bool fwsub (unsigned sig, Cls *);
  bool bwsub (unsigned sig, Cls *);
  void assign (int l);
  void assume (int l);
  void imply (int l, int reason);
  int dominator (int l, Cls * reason, bool &);
  void force (int l, Cls * reason);
  void unit (int l);
  bool min2 (int lit, int other, int depth);
  bool minl (int lit, Cls *, int depth);
  bool strengthen (int lit, int depth);
  bool minimize (Var *, int depth);
  int luby (int);
  void report (bool v, char ch);
  void prop2 (int lit);
  void propl (int lit);
  bool bcp ();
  void touch (int lit);
  void touch (Cls *);
  void bump (Var *);
  int phase (Var *);
  bool decide ();
  void extend ();
  void probe ();
  int find (int lit);
  void merge (int, int, int & merged);
  void shrink (int);
  void enlarge ();
  void recompeqstats ();
  void decompose ();
  bool resolve (int l, int pivot, int k, bool tryonly);
  bool resolve (int l, int pivot, Cls *, bool tryonly);
  bool resolve (Cls *, int pivot, Cls *, bool tryonly);
  bool andgate (int lit);
  Cls * find (int a, int b, int c);
  int itegate (int lit, int cond, int t);
  bool itegate (int lit);
  bool xorgate (int lit);
  bool hasgate (int idx);
  bool trelim (int idx);
  void elim (int idx);
  void fwbw ();
  void elim ();
  void cleantrash ();
  void cleangate ();
  void dump (Cls *);
  void gcls (Cls *);
  void gc (Anchor<Cls> &, const char*);
  void gc ();
  void jwh (Cls *);
  void jwh ();
  void reduce ();
  void simplify ();
  void iteration ();
  void restart ();
  void rebias ();
  void undo (int newlevel, bool save = false);
  void pull (int lit);
  bool analyze ();
  void flush ();

  void checkeliminated ();
  void checkclean ();

  void import ();
  int search ();
  bool satisfied (Anchor<Cls> &);

  void print (const char *, Anchor<Cls>& ); // TODO remove
  void print (); // TODO remove

  void printgate ();
  void checkgate ();

public:

  Solver (int m);
  bool set (const char * option, int arg) { return opts.set (option, arg); }
  void add (int lit) { if (lit) lits.push (mem, lit); else import (); }
  int solve (int decision_limit = UINT_MAX);
  int val (int l) { return vals[find (l)]; }
  double seconds () { return stats.seconds (); }
  bool satisfied ();
  void prstats ();
  void propts ();
  void fxopts ();
  ~Solver ();
};

};

#endif
