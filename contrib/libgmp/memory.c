/* Memory allocation routines.

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

#include <stdio.h>

#include "gmp.h"
#include "gmp-impl.h"

#ifdef __NeXT__
#define static
#endif

#if __STDC__
void *	(*_mp_allocate_func) (size_t) = _mp_default_allocate;
void *	(*_mp_reallocate_func) (void *, size_t, size_t)
     = _mp_default_reallocate;
void	(*_mp_free_func) (void *, size_t) = _mp_default_free;
#else
void *	(*_mp_allocate_func) () = _mp_default_allocate;
void *	(*_mp_reallocate_func) () = _mp_default_reallocate;
void	(*_mp_free_func) () = _mp_default_free;
#endif

/* Default allocation functions.  In case of failure to allocate/reallocate
   an error message is written to stderr and the program aborts.  */

void *
#if __STDC__
_mp_default_allocate (size_t size)
#else
_mp_default_allocate (size)
     size_t size;
#endif
{
  void *ret;

  ret = malloc (size);
  if (ret == 0)
    {
      perror ("cannot allocate in gmp");
      abort ();
    }

  return ret;
}

void *
#if __STDC__
_mp_default_reallocate (void *oldptr, size_t old_size, size_t new_size)
#else
_mp_default_reallocate (oldptr, old_size, new_size)
     void *oldptr;
     size_t old_size;
     size_t new_size;
#endif
{
  void *ret;

  ret = realloc (oldptr, new_size);
  if (ret == 0)
    {
      perror ("cannot allocate in gmp");
      abort ();
    }

  return ret;
}

void
#if __STDC__
_mp_default_free (void *blk_ptr, size_t blk_size)
#else
_mp_default_free (blk_ptr, blk_size)
     void *blk_ptr;
     size_t blk_size;
#endif
{
  free (blk_ptr);
}
