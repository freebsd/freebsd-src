/* mpz_sizeinbase(x, base) -- return an approximation to the number of
   character the integer X would have printed in base BASE.  The
   approximation is never too small.

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
#include "longlong.h"

size_t
#if __STDC__
mpz_sizeinbase (mpz_srcptr x, int base)
#else
mpz_sizeinbase (x, base)
     mpz_srcptr x;
     int base;
#endif
{
  mp_size_t size = ABS (x->_mp_size);
  int lb_base, cnt;
  size_t totbits;

  /* Special case for X == 0.  */
  if (size == 0)
    return 1;

  /* Calculate the total number of significant bits of X.  */
  count_leading_zeros (cnt, x->_mp_d[size - 1]);
  totbits = size * BITS_PER_MP_LIMB - cnt;

  if ((base & (base - 1)) == 0)
    {
      /* Special case for powers of 2, giving exact result.  */

      count_leading_zeros (lb_base, base);
      lb_base = BITS_PER_MP_LIMB - lb_base - 1;

      return (totbits + lb_base - 1) / lb_base;
    }
  else
    return (size_t) (totbits * __mp_bases[base].chars_per_bit_exactly) + 1;
}
