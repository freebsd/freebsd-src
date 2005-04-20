/* xmalloc.c -- malloc with out of memory checking
   Copyright (C) 1990, 91, 92, 93, 94 Free Software Foundation, Inc.

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

/* This is for other GNU distributions with internationalized messages.
   The GNU C Library itself does not yet support such messages.  */
#if HAVE_LIBINTL_H
# include <libintl.h>
#else
# define gettext(msgid) (msgid)
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

/* Exit value when the requested amount of memory is not available.
   The caller may set it to some other value.  */
int xmalloc_exit_failure = EXIT_FAILURE;

#if __STDC__ && (HAVE_VPRINTF || HAVE_DOPRNT)
void error (int, int, const char *, ...);
#else
void error ();
#endif

static VOID *
fixup_null_alloc (n)
     size_t n;
{
  VOID *p;

  p = 0;
  if (n == 0)
    p = malloc ((size_t) 1);
  if (p == 0)
    error (xmalloc_exit_failure, 0, gettext ("Memory exhausted"));
  return p;
}

/* Allocate N bytes of memory dynamically, with error checking.  */

VOID *
xmalloc (n)
     size_t n;
{
  VOID *p;

  p = malloc (n);
  if (p == 0)
    p = fixup_null_alloc (n);
  return p;
}

/* Change the size of an allocated block of memory P to N bytes,
   with error checking.
   If P is NULL, run xmalloc.  */

VOID *
xrealloc (p, n)
     VOID *p;
     size_t n;
{
  if (p == 0)
    return xmalloc (n);
  p = realloc (p, n);
  if (p == 0)
    p = fixup_null_alloc (n);
  return p;
}
