/* mpn_preinv_mod_1 (dividend_ptr, dividend_size, divisor_limb,
		       divisor_limb_inverted) --
   Divide (DIVIDEND_PTR,,DIVIDEND_SIZE) by the normalized DIVISOR_LIMB.
   DIVISOR_LIMB_INVERTED should be 2^(2*BITS_PER_MP_LIMB) / DIVISOR_LIMB +
   - 2^BITS_PER_MP_LIMB.
   Return the single-limb remainder.

Copyright (C) 1991, 1993, 1994, Free Software Foundation, Inc.

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

#ifndef UMUL_TIME
#define UMUL_TIME 1
#endif

#ifndef UDIV_TIME
#define UDIV_TIME UMUL_TIME
#endif

mp_limb_t
#if __STDC__
mpn_preinv_mod_1 (mp_srcptr dividend_ptr, mp_size_t dividend_size,
		  mp_limb_t divisor_limb, mp_limb_t divisor_limb_inverted)
#else
mpn_preinv_mod_1 (dividend_ptr, dividend_size, divisor_limb, divisor_limb_inverted)
     mp_srcptr dividend_ptr;
     mp_size_t dividend_size;
     mp_limb_t divisor_limb;
     mp_limb_t divisor_limb_inverted;
#endif
{
  mp_size_t i;
  mp_limb_t n0, r;
  int dummy;

  i = dividend_size - 1;
  r = dividend_ptr[i];

  if (r >= divisor_limb)
    r = 0;
  else
    i--;

  for (; i >= 0; i--)
    {
      n0 = dividend_ptr[i];
      udiv_qrnnd_preinv (dummy, r, r, n0, divisor_limb, divisor_limb_inverted);
    }
  return r;
}
