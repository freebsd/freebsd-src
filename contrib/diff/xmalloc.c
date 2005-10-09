/* xmalloc.c -- malloc with out of memory checking
   Copyright (C) 1990, 1991, 1993 Free Software Foundation, Inc.

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
#include <config.h>
#endif

#if __STDC__
#define VOID void
#else
#define VOID char
#endif

#include <sys/types.h>

#if STDC_HEADERS
#include <stdlib.h>
#else
VOID *malloc ();
VOID *realloc ();
void free ();
#endif

#if __STDC__ && defined (HAVE_VPRINTF)
void error (int, int, char const *, ...);
#else
void error ();
#endif

/* Allocate N bytes of memory dynamically, with error checking.  */

VOID *
xmalloc (n)
     size_t n;
{
  VOID *p;

  p = malloc (n);
  if (p == 0)
    /* Must exit with 2 for `cmp'.  */
    error (2, 0, "memory exhausted");
  return p;
}

/* Change the size of an allocated block of memory P to N bytes,
   with error checking.
   If P is NULL, run xmalloc.
   If N is 0, run free and return NULL.  */

VOID *
xrealloc (p, n)
     VOID *p;
     size_t n;
{
  if (p == 0)
    return xmalloc (n);
  if (n == 0)
    {
      free (p);
      return 0;
    }
  p = realloc (p, n);
  if (p == 0)
    /* Must exit with 2 for `cmp'.  */
    error (2, 0, "memory exhausted");
  return p;
}
