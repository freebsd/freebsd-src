/* mpz_init_set_d(integer, val) -- Initialize and assign INTEGER with a double
   value VAL.

Copyright (C) 1996 Free Software Foundation, Inc.

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
mpz_init_set_d (mpz_ptr dest, double val)
#else
mpz_init_set_d (dest, val)
     mpz_ptr dest;
     double val;
#endif
{
  dest->_mp_alloc = 1;
  dest->_mp_d = (mp_ptr) (*_mp_allocate_func) (BYTES_PER_MP_LIMB);
  dest->_mp_size = 0;
  mpz_set_d (dest, val);
}
