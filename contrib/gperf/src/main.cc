/* Driver program for the Gen_Perf hash function generator
   Copyright (C) 1989-1998, 2000 Free Software Foundation, Inc.
   written by Douglas C. Schmidt (schmidt@ics.uci.edu)

This file is part of GNU GPERF.

GNU GPERF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU GPERF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU GPERF; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111, USA.  */

/* Simple driver program for the Gen_Perf.hash function generator.
   Most of the hard work is done in class Gen_Perf and its class methods. */

#include "config.h"
#include <sys/types.h>
#if LARGE_STACK_ARRAYS && defined(HAVE_GETRLIMIT) && defined(HAVE_SETRLIMIT)
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif

#include <stdio.h>
#include "options.h"
#include "gen-perf.h"
#include "trace.h"

int
main (int argc, char *argv[])
{
  T (Trace t ("main");)

#if LARGE_STACK_ARRAYS && defined(HAVE_GETRLIMIT) && defined(HAVE_SETRLIMIT) && defined(RLIMIT_STACK)
  /* Get rid of any avoidable limit on stack size.  */
  {
    struct rlimit rlim;
    if (getrlimit (RLIMIT_STACK, &rlim) == 0)
      if (rlim.rlim_cur < rlim.rlim_max)
        {
          rlim.rlim_cur = rlim.rlim_max;
          setrlimit (RLIMIT_STACK, &rlim);
        }
  }
#endif /* RLIMIT_STACK */

  /* Sets the Options. */
  option (argc, argv);

  /* Initializes the key word list. */
  Gen_Perf generate_table;

  /* Generates and prints the Gen_Perf hash table. */
  int status = generate_table ();

  /* Check for write error on stdout. */
  if (fflush (stdout) || ferror (stdout))
    status = 1;

  /* Don't use exit() here, it skips the destructors. */
  return status;
}
