/* mpz_get_si(integer) -- Return the least significant digit from INTEGER.

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

signed long int
#if __STDC__
mpz_get_si (mpz_srcptr op)
#else
mpz_get_si (op)
     mpz_srcptr op;
#endif
{
  mp_size_t size = op->_mp_size;
  mp_limb_t low_limb = op->_mp_d[0];

  if (size > 0)
    return low_limb % ((mp_limb_t) 1 << (BITS_PER_MP_LIMB - 1));
  else if (size < 0)
    /* This convoluted expression is necessary to properly handle 0x80000000 */
    return ~((low_limb - 1) % ((mp_limb_t) 1 << (BITS_PER_MP_LIMB - 1)));
  else
    return 0;
}
