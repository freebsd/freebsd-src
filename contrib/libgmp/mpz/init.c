/* mpz_init() -- Make a new multiple precision number with value 0.

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

void
#if __STDC__
mpz_init (mpz_ptr x)
#else
mpz_init (x)
     mpz_ptr x;
#endif
{
  x->_mp_alloc = 1;
  x->_mp_d = (mp_ptr) (*_mp_allocate_func) (BYTES_PER_MP_LIMB);
  x->_mp_size = 0;
}
