/* Replacement for getopt() that can be used by tar.
   Copyright (C) 1988 Free Software Foundation

This file is part of GNU Tar.

GNU Tar is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Tar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Tar; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * Plug-compatible replacement for getopt() for parsing tar-like
 * arguments.  If the first argument begins with "-", it uses getopt;
 * otherwise, it uses the old rules used by tar, dump, and ps.
 *
 * Written 25 August 1985 by John Gilmore (ihnp4!hoptoad!gnu)
 */

#include <stdio.h>
#include "getopt.h"
#include "tar.h"		/* For msg() declaration if STDC_MSG. */
#include <sys/types.h>
#include "port.h"

int
getoldopt (argc, argv, optstring, long_options, opt_index)
     int argc;
     char **argv;
     char *optstring;
     struct option *long_options;
     int *opt_index;
{
  extern char *optarg;		/* Points to next arg */
  extern int optind;		/* Global argv index */
  static char *key;		/* Points to next keyletter */
  static char use_getopt;	/* !=0 if argv[1][0] was '-' */
  char c;
  char *place;

  optarg = NULL;

  if (key == NULL)
    {				/* First time */
      if (argc < 2)
	return EOF;
      key = argv[1];
      if ((*key == '-') || (*key == '+'))
	use_getopt++;
      else
	optind = 2;
    }

  if (use_getopt)
    return getopt_long (argc, argv, optstring,
			long_options, opt_index);

  c = *key++;
  if (c == '\0')
    {
      key--;
      return EOF;
    }
  place = index (optstring, c);

  if (place == NULL || c == ':')
    {
      msg ("unknown option %c", c);
      return ('?');
    }

  place++;
  if (*place == ':')
    {
      if (optind < argc)
	{
	  optarg = argv[optind];
	  optind++;
	}
      else
	{
	  msg ("%c argument missing", c);
	  return ('?');
	}
    }

  return (c);
}
