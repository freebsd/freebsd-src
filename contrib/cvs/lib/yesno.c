/* yesno.c -- read a yes/no response from stdin
   Copyright (C) 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

/* Read one line from standard input
   and return nonzero if that line begins with y or Y,
   otherwise return 0. */

int
yesno ()
{
  int c;
  int rv;

  fflush (stderr);
  fflush (stdout);
  c = getchar ();
  rv = (c == 'y') || (c == 'Y');
  while (c != EOF && c != '\n')
    c = getchar ();

  return rv;
}
