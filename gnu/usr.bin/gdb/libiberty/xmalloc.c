/* memory allocation routines with error checking.
   Copyright 1989, 1991, 1993 Free Software Foundation, Inc.
   
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

#include <ansidecl.h>

#include <stdio.h>

#ifdef __STDC__
#include <stddef.h>
#else
#define size_t unsigned long
#endif


PTR
xmalloc (size)
    size_t size;
{
  char * newmem;

  if ((newmem = (char *) malloc ((int) size)) == NULL)
    {
      fprintf (stderr, "\nCan't allocate %u bytes\n", size);
      exit (1);
    }
  return (newmem);
}

PTR
xrealloc (oldmem, size)
    PTR oldmem;
    size_t size;
{
  char * newmem;

  if ((newmem = (char *) realloc ((char *) oldmem, (int) size)) == NULL)
    {
      fprintf (stderr, "\nCan't reallocate %u bytes\n", size);
      exit (1);
    }
  return (newmem);
}
