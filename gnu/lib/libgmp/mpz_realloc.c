/* _mpz_realloc -- make the MP_INT have NEW_SIZE digits allocated.

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

#include "gmp.h"
#include "gmp-impl.h"

void *
#ifdef __STDC__
_mpz_realloc (MP_INT *m, mp_size new_size)
#else
_mpz_realloc (m, new_size)
     MP_INT *m;
     mp_size new_size;
#endif
{
  /* Never allocate zero space. */
  if (new_size == 0)
    new_size = 1;

  m->d = (mp_ptr) (*_mp_reallocate_func) (m->d, m->alloc * BYTES_PER_MP_LIMB,
					  new_size * BYTES_PER_MP_LIMB);
  m->alloc = new_size;

#if 0
  /* This might break some code that reads the size field after
     reallocation, in the case the reallocated destination and a
     source argument are identical.  */
  if (ABS (m->size) > new_size)
    m->size = 0;
#endif

  return (void *) m->d;
}
