/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#include <string.h>	/* Prototypes for memcpy, memmove, memset, etc */

#include "mmalloc.h"

/* Allocate an array of NMEMB elements each SIZE bytes long.
   The entire array is initialized to zeros.  */

PTR
mcalloc (md, nmemb, size)
  PTR md;
  register size_t nmemb;
  register size_t size;
{
  register PTR result;

  if ((result = mmalloc (md, nmemb * size)) != NULL)
    {
      memset (result, 0, nmemb * size);
    }
  return (result);
}

/* When using this package, provide a version of malloc/realloc/free built
   on top of it, so that if we use the default sbrk() region we will not
   collide with another malloc package trying to do the same thing, if
   the application contains any "hidden" calls to malloc/realloc/free (such
   as inside a system library). */

PTR
calloc (nmemb, size)
  size_t nmemb;
  size_t size;
{
  return (mcalloc ((PTR) NULL, nmemb, size));
}
