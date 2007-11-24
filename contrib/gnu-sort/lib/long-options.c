/* Utility to accept --help and --version options as unobtrusively as possible.

   Copyright (C) 1993, 1994, 1998, 1999, 2000, 2002, 2003, 2004 Free
   Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Jim Meyering.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* Specification.  */
#include "long-options.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "version-etc.h"

static struct option const long_options[] =
{
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'v'},
  {0, 0, 0, 0}
};

/* Process long options --help and --version, but only if argc == 2.
   Be careful not to gobble up `--'.  */

void
parse_long_options (int argc,
		    char **argv,
		    const char *command_name,
		    const char *package,
		    const char *version,
		    void (*usage_func) (int),
		    /* const char *author1, ...*/ ...)
{
  int c;
  int saved_opterr;

  saved_opterr = opterr;

  /* Don't print an error message for unrecognized options.  */
  opterr = 0;

  if (argc == 2
      && (c = getopt_long (argc, argv, "+", long_options, NULL)) != -1)
    {
      switch (c)
	{
	case 'h':
	  (*usage_func) (EXIT_SUCCESS);

	case 'v':
	  {
	    va_list authors;
	    va_start (authors, usage_func);
	    version_etc_va (stdout, command_name, package, version, authors);
	    exit (0);
	  }

	default:
	  /* Don't process any other long-named options.  */
	  break;
	}
    }

  /* Restore previous value.  */
  opterr = saved_opterr;

  /* Reset this to zero so that getopt internals get initialized from
     the probably-new parameters when/if getopt is called later.  */
  optind = 0;
}
