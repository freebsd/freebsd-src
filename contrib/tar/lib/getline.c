/* getline.c -- Replacement for GNU C library function getline

Copyright (C) 1993, 1996, 1997, 1998, 2000 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/* Written by Jan Brittenson, bson@gnu.ai.mit.edu.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* The `getdelim' function is only declared if the following symbol
   is defined.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif

#include <stdio.h>
#include <sys/types.h>

#if defined __GNU_LIBRARY__ && HAVE_GETDELIM

int
getline (char **lineptr, size_t *n, FILE *stream)
{
  return getdelim (lineptr, n, '\n', stream);
}

#else /* ! have getdelim */

# include "getstr.h"

int
getline (char **lineptr, size_t *n, FILE *stream)
{
  return getstr (lineptr, n, stream, '\n', 0, 0);
}

int
getdelim (char **lineptr, size_t *n, int delimiter, FILE *stream)
{
  return getstr (lineptr, n, stream, delimiter, 0, 0);
}
#endif
