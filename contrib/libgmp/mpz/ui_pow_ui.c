/* mpz_ui_pow_ui(res, base, exp) -- Set RES to BASE**EXP.

Copyright (C) 1991, 1993, 1994, 1996 Free Software Foundation, Inc.

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

void
#if __STDC__
mpz_ui_pow_ui (mpz_ptr r, unsigned long int b, unsigned long int e)
#else
mpz_ui_pow_ui (r, b, e)
     mpz_ptr r;
     unsigned long int b;
     unsigned long int e;
#endif
{
  mp_ptr rp, tp, xp;
  mp_size_t rsize;
  int cnt, i;
  mp_limb_t blimb = b;
  TMP_DECL (marker);

  /* Single out cases that give result == 0 or 1.  These tests are here
     to simplify the general code below, not to optimize.  */
  if (e == 0)
    {
      r->_mp_d[0] = 1;
      r->_mp_size = 1;
      return;
    }
  if (blimb == 0)
    {
      r->_mp_size = 0;
      return;
    }

  if (blimb < 0x100)
    {
      /* Estimate space requirements accurately.  Using the code from the
	 `else' path would over-estimate space requirements wildly.   */
      float lb = __mp_bases[blimb].chars_per_bit_exactly;
      rsize = 2 + ((mp_size_t) (e / lb) / BITS_PER_MP_LIMB);
    }
  else
    {
      /* Over-estimate space requirements somewhat.  */
      count_leading_zeros (cnt, blimb);
      rsize = e - cnt * e / BITS_PER_MP_LIMB + 1;
    }

  TMP_MARK (marker);

  /* The two areas are used to alternatingly hold the input and recieve the
     product for mpn_mul.  (This scheme is used to fulfill the requirements
     of mpn_mul; that the product space may not be the same as any of the
     input operands.)  */
  rp = (mp_ptr) TMP_ALLOC (rsize * BYTES_PER_MP_LIMB);
  tp = (mp_ptr) TMP_ALLOC (rsize * BYTES_PER_MP_LIMB);

  rp[0] = blimb;
  rsize = 1;
  count_leading_zeros (cnt, e);

  for (i = BITS_PER_MP_LIMB - cnt - 2; i >= 0; i--)
    {
      mpn_mul_n (tp, rp, rp, rsize);
      rsize = 2 * rsize;
      rsize -= tp[rsize - 1] == 0;
      xp = tp; tp = rp; rp = xp;

      if ((e & ((mp_limb_t) 1 << i)) != 0)
	{
	  mp_limb_t cy;
	  cy = mpn_mul_1 (tp, rp, rsize, blimb);
	  if (cy != 0)
	    {
	      tp[rsize] = cy;
	      rsize++;
	    }
	  xp = tp; tp = rp; rp = xp;
	}
    }

  /* Now then we know the exact space requirements, reallocate if
     necessary.  */
  if (r->_mp_alloc < rsize)
    _mpz_realloc (r, rsize);

  MPN_COPY (r->_mp_d, rp, rsize);
  r->_mp_size = rsize;
  TMP_FREE (marker);
}
