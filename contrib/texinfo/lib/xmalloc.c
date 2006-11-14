/* xmalloc.c -- safe versions of malloc and realloc.

   Copyright (C) 1988, 1989, 1990, 1991, 1992, 1993, 2004 Free Software
   Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#if !defined (ALREADY_HAVE_XMALLOC)
#include "system.h"

static void
memory_error_and_abort (const char *fname)
{
  fprintf (stderr, "%s: Out of virtual memory!\n", fname);
  abort ();
}

/* Return a pointer to free()able block of memory large enough
   to hold BYTES number of bytes.  If the memory cannot be allocated,
   print an error message and abort. */
void *
xmalloc (size_t bytes)
{
  void *temp = malloc (bytes);

  if (!temp)
    memory_error_and_abort ("xmalloc");
  return (temp);
}

void *
xrealloc (void *pointer, size_t bytes)
{
  void *temp;

  if (!pointer)
    temp = malloc (bytes);
  else
    temp = realloc (pointer, bytes);

  if (!temp)
    memory_error_and_abort ("xrealloc");

  return (temp);
}

#endif /* !ALREADY_HAVE_XMALLOC */
