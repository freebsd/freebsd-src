/* memory allocation routines with error checking.
   Copyright 1989, 90, 91, 92, 93, 94 Free Software Foundation, Inc.
   
This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#include "ansidecl.h"
#include "libiberty.h"

#include <stdio.h>

#ifdef __STDC__
#include <stddef.h>
#else
#define size_t unsigned long
#endif

/* For systems with larger pointers than ints, these must be declared.  */
PTR malloc PARAMS ((size_t));
PTR realloc PARAMS ((PTR, size_t));

/* The program name if set.  */
static const char *name = "";

void
xmalloc_set_program_name (s)
     const char *s;
{
  name = s;
}

PTR
xmalloc (size)
    size_t size;
{
  PTR newmem;

  if (size == 0)
    size = 1;
  newmem = malloc (size);
  if (!newmem)
    {
      fprintf (stderr, "\n%s%sCan not allocate %lu bytes\n",
	       name, *name ? ": " : "",
	       (unsigned long) size);
      xexit (1);
    }
  return (newmem);
}

PTR
xrealloc (oldmem, size)
    PTR oldmem;
    size_t size;
{
  PTR newmem;

  if (size == 0)
    size = 1;
  if (!oldmem)
    newmem = malloc (size);
  else
    newmem = realloc (oldmem, size);
  if (!newmem)
    {
      fprintf (stderr, "\n%s%sCan not reallocate %lu bytes\n",
	       name, *name ? ": " : "",
	       (unsigned long) size);
      xexit (1);
    }
  return (newmem);
}
