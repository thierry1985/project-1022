#include "precobnr.hh"
#include "precocfg.hh"

#include <stdio.h>
#include <assert.h>

void precosat_banner () {
  printf ("c PicoSATLight Version %s %s\n", PRECOSAT_VERSION, PRECOSAT_ID);
  printf ("c Copyright (C) 2009, Armin Biere, JKU, Linz, Austria."
          "  All rights reserved.\n");
  printf ("c Released on %s\n", PRECOSAT_RELEASED);
  printf ("c Compiled on %s\n", PRECOSAT_COMPILED);
  printf ("c %s\n", PRECOSAT_CXX);
  const char * p = PRECOSAT_CXXFLAGS;
  assert (*p);
  for (;;) {
    fputs ("c ", stdout);
    const char * q;
    for (q = p; *q && *q != ' '; q++)
      ;
    if (*q && q - p < 76) {
      for (;;) {
	const char * n;
	for (n = q + 1; *n && *n != ' '; n++)
	  ;
	if (n - p >= 76) break;
	q = n;
	if (!*n) break;
      }
      while (p < q) fputc (*p++, stdout);
      fputc ('\n', stdout);
      if (!*p) break;
      assert(*p == ' ');
      p++;
    }
  }
  printf ("c %s\n", PRECOSAT_OS);
  fflush (stdout);
}
