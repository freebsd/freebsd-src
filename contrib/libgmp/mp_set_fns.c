/* mp_set_memory_functions -- Set the allocate, reallocate, and free functions
   for use by the mp package.

Copyright (C) 1991, 1993, 1994 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
License for more details.

You should have received a copy of the GNU Library General Public License
along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA. */

#include "gmp.h"
#include "gmp-impl.h"

void
#if __STDC__
mp_set_memory_functions (void *(*alloc_func) (size_t),
			 void *(*realloc_func) (void *, size_t, size_t),
			 void (*free_func) (void *, size_t))
#else
mp_set_memory_functions (alloc_func, realloc_func, free_func)
     void *(*alloc_func) ();
     void *(*realloc_func) ();
     void (*free_func) ();
#endif
{
  if (alloc_func == 0)
    alloc_func = _mp_default_allocate;
  if (realloc_func == 0)
    realloc_func = _mp_default_reallocate;
  if (free_func == 0)
    free_func = _mp_default_free;

  _mp_allocate_func = alloc_func;
  _mp_reallocate_func = realloc_func;
  _mp_free_func = free_func;
}
