/* _mpz_realloc -- make the mpz_t have NEW_SIZE digits allocated.

Copyright (C) 1991, 1993, 1994, 1995 Free Software Foundation, Inc.

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

void *
#if __STDC__
_mpz_realloc (mpz_ptr m, mp_size_t new_size)
#else
_mpz_realloc (m, new_size)
     mpz_ptr m;
     mp_size_t new_size;
#endif
{
  /* Never allocate zero space. */
  if (new_size == 0)
    new_size = 1;

  m->_mp_d = (mp_ptr) (*_mp_reallocate_func) (m->_mp_d,
					      m->_mp_alloc * BYTES_PER_MP_LIMB,
					      new_size * BYTES_PER_MP_LIMB);
  m->_mp_alloc = new_size;

#if 0
  /* This might break some code that reads the size field after
     reallocation, in the case the reallocated destination and a
     source argument are identical.  */
  if (ABS (m->_mp_size) > new_size)
    m->_mp_size = 0;
#endif

  return (void *) m->_mp_d;
}
