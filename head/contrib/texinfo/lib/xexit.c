/* xexit.c -- exit with attention to return values and closing stdout.
   $Id: xexit.c,v 1.5 2004/04/11 17:56:46 karl Exp $

   Copyright (C) 1999, 2003, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "system.h"

/* SunOS 4.1.1 gets STDC_HEADERS defined, but it doesn't provide
   EXIT_FAILURE.  So far no system has defined one of EXIT_FAILURE and
   EXIT_SUCCESS without the other.  */
#ifdef EXIT_SUCCESS
 /* The following test is to work around the gross typo in
    systems like Sony NEWS-OS Release 4.0C, whereby EXIT_FAILURE
    is defined to 0, not 1.  */
# if !EXIT_FAILURE
#  undef EXIT_FAILURE
#  define EXIT_FAILURE 1
# endif
#else /* not EXIT_SUCCESS */
# ifdef VMS /* these values suppress some messages; from gnuplot */
#   define EXIT_SUCCESS 1
#   define EXIT_FAILURE 0x10000002
# else /* not VMS */
#  define EXIT_SUCCESS 0
#  define EXIT_FAILURE 1
# endif /* not VMS */
#endif /* not EXIT_SUCCESS */


/* Flush stdout first, exit if failure (therefore, xexit should be
   called to exit every program, not just `return' from main).
   Otherwise, if EXIT_STATUS is zero, exit successfully, else
   unsuccessfully.  */

void
xexit (int exit_status)
{
  if (ferror (stdout))
    {
      fputs (_("ferror on stdout\n"), stderr);
      exit_status = 1;
    }
  else if (fflush (stdout) != 0)
    {
      fputs (_("fflush error on stdout\n"), stderr);
      exit_status = 1;
    }

  exit_status = exit_status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

  exit (exit_status);
}


/* Why do we care about stdout you may ask?  Here's why, from Jim
   Meyering in the lib/closeout.c file.  */

/* If a program writes *anything* to stdout, that program should close
   stdout and make sure that the close succeeds.  Otherwise, suppose that
   you go to the extreme of checking the return status of every function
   that does an explicit write to stdout.  The last printf can succeed in
   writing to the internal stream buffer, and yet the fclose(stdout) could
   still fail (due e.g., to a disk full error) when it tries to write
   out that buffered data.  Thus, you would be left with an incomplete
   output file and the offending program would exit successfully.

   Besides, it's wasteful to check the return value from every call
   that writes to stdout -- just let the internal stream state record
   the failure.  That's what the ferror test is checking below.

   It's important to detect such failures and exit nonzero because many
   tools (most notably `make' and other build-management systems) depend
   on being able to detect failure in other tools via their exit status.  */
