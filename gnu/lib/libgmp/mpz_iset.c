/* mpz_init_set (src_integer) -- Make a new multiple precision number with
   a value copied from SRC_INTEGER.

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

void
#ifdef __STDC__
mpz_init_set (MP_INT *x, const MP_INT *src)
#else
mpz_init_set (x, src)
     MP_INT *x;
     const MP_INT *src;
#endif
{
  mp_size size;
  mp_size abs_size;

  size = src->size;
  abs_size = ABS (size);

  x->alloc = abs_size == 0 ? 1 : abs_size;
  x->d = (mp_ptr) (*_mp_allocate_func) (BYTES_PER_MP_LIMB * x->alloc);

  MPN_COPY (x->d, src->d, abs_size);
  x->size = size;
}
