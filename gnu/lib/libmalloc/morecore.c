/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the GNU C Library; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef	_MALLOC_INTERNAL
#define	_MALLOC_INTERNAL
#include <malloc.h>
#endif

#ifndef	__GNU_LIBRARY__
#define	__sbrk	sbrk
#endif

extern __ptr_t __sbrk __P ((int increment));

#ifndef NULL
#define NULL 0
#endif

/* Allocate INCREMENT more bytes of data space,
   and return the start of data space, or NULL on errors.
   If INCREMENT is negative, shrink data space.  */
__ptr_t
__default_morecore (increment)
     ptrdiff_t increment;
{
  __ptr_t result = __sbrk ((int) increment);
  if (result == (__ptr_t) -1)
    return NULL;
  return result;
}
