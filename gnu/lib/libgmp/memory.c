/* Memory allocation routines.

Copyright (C) 1991 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

The GNU MP Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the GNU MP Library; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>

#include "gmp.h"
#include "gmp-impl.h"

#ifdef __NeXT__
#define static
#endif

#ifdef __STDC__
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
#ifdef __STDC__
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
      perror ("cannot allocate in libmp");
      abort ();
    }

  return ret;
}

void *
#ifdef __STDC__
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
      perror ("cannot allocate in libmp");
      abort ();
    }

  return ret;
}

void
#ifdef __STDC__
_mp_default_free (void *blk_ptr, size_t blk_size)
#else
_mp_default_free (blk_ptr, blk_size)
     void *blk_ptr;
     size_t blk_size;
#endif
{
  free (blk_ptr);
}
