/* Copyright (C) 2004
   Free Software Foundation, Inc.
     Written by: Keith Marshall (keith.d.marshall@ntlworld.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_PROCESS_H
# include <process.h>
#endif

#if defined(__MSDOS__) \
    || (defined(_WIN32) && !defined(_UWIN) && !defined(__CYGWIN__)) \
    || defined(__EMX__)

#define SPAWN_FUNCTION_WRAPPERS 1

/* Define the default mechanism, and messages, for error reporting
 * (user may substitute a preferred alternative, by defining his own
 *  implementation of the macros REPORT_ERROR and ARGV_MALLOC_ERROR,
 *  in the header file `nonposix.h').
 */

#include "nonposix.h"

#ifndef  REPORT_ERROR
# define REPORT_ERROR(WHY)  fprintf(stderr, "%s:%s\n", program_name, WHY)
#endif
#ifndef  ARGV_MALLOC_ERROR
# define ARGV_MALLOC_ERROR    "malloc: Allocation for 'argv' failed"
#endif

extern char *program_name;

extern char *quote_arg(char *string);
extern void purge_quoted_args(char **argv);

int
spawnvp_wrapper(int mode, char *path, char **argv)
{
  /* Invoke the system `spawnvp' service
   * enclosing the passed arguments in double quotes, as required,
   * so that the (broken) default parsing in the MSVC runtime doesn't
   * split them at whitespace. */

  char **quoted_argv;	/* used to build a quoted local copy of `argv' */

  int i;		/* used as an index into `argv' or `quoted_argv' */
  int status = -1;	/* initialise return code, in case we fail */
  int argc = 0;		/* initialise argument count; may be none  */

  /* First count the number of arguments
   * which are actually present in the passed `argv'. */

  if (argv)
    for (quoted_argv = argv; *quoted_argv; ++argc, ++quoted_argv)
      ;

  /* If we do not now have an argument count,
   * then we must fall through and fail. */
  
  if (argc) {
    /* We do have at least one argument:
     * We will use a copy of the `argv', in which to do the quoting,
     * so we must allocate space for it. */

    if ((quoted_argv = (char **)malloc(++argc * sizeof(char **))) == NULL) {
      /* If we didn't get enough space,
       * then complain, and bail out gracefully. */

      REPORT_ERROR(ARGV_MALLOC_ERROR);
      exit(1);
    }

    /* Now copy the passed `argv' into our new vector,
     * quoting its contents as required. */
    
    for (i = 0; i < argc; i++)
      quoted_argv[i] = quote_arg(argv[i]);

    /* Invoke the MSVC `spawnvp' service
     * passing our now appropriately quoted copy of `argv'. */

    status = spawnvp(mode, path, quoted_argv);

    /* Clean up our memory allocations
     * for the quoted copy of `argv', which is no longer required. */

    purge_quoted_args(quoted_argv);
    free(quoted_argv);
  }

  /* Finally,
   * return the status code returned by `spawnvp',
   * or a failure code if we fell through. */

  return status;
}

#endif  /* __MSDOS__ || _WIN32 */

/* spawnvp.c: end of file */
