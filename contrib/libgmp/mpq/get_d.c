/* double mpq_get_d (mpq_t src) -- Return the double approximation to SRC.

Copyright (C) 1995, 1996 Free Software Foundation, Inc.

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

/* Algorithm:
   1. Develop >= n bits of src.num / src.den, where n is the number of bits
      in a double.  This (partial) division will use all bits from the
      denominator.
   2. Use the remainder to determine how to round the result.
   3. Assign the integral result to a temporary double.
   4. Scale the temporary double, and return the result.

   An alternative algorithm, that would be faster:
   0. Let n be somewhat larger than the number of significant bits in a double.
   1. Extract the most significant n bits of the denominator, and an equal
      number of bits from the numerator.
   2. Interpret the extracted numbers as integers, call them a and b
      respectively, and develop n bits of the fractions ((a + 1) / b) and
      (a / (b + 1)) using mpn_divrem.
   3. If the computed values are identical UP TO THE POSITION WE CARE ABOUT,
      we are done.  If they are different, repeat the algorithm from step 1,
      but first let n = n * 2.
   4. If we end up using all bits from the numerator and denominator, fall
      back to the first algorithm above.
   5. Just to make life harder, The computation of a + 1 and b + 1 above
      might give carry-out...  Needs special handling.  It might work to
      subtract 1 in both cases instead.
*/

double
#if __STDC__
mpq_get_d (const MP_RAT *src)
#else
mpq_get_d (src)
     const MP_RAT *src;
#endif
{
  mp_ptr np, dp;
  mp_ptr rp;
  mp_size_t nsize = src->_mp_num._mp_size;
  mp_size_t dsize = src->_mp_den._mp_size;
  mp_size_t qsize, rsize;
  mp_size_t sign_quotient = nsize ^ dsize;
  unsigned normalization_steps;
  mp_limb_t qlimb;
#define N_QLIMBS (1 + (sizeof (double) + BYTES_PER_MP_LIMB-1) / BYTES_PER_MP_LIMB)
  mp_limb_t qp[N_QLIMBS + 1];
  TMP_DECL (marker);

  if (nsize == 0)
    return 0.0;

  TMP_MARK (marker);
  nsize = ABS (nsize);
  dsize = ABS (dsize);
  np = src->_mp_num._mp_d;
  dp = src->_mp_den._mp_d;

  rsize = dsize + N_QLIMBS;
  rp = (mp_ptr) TMP_ALLOC ((rsize + 1) * BYTES_PER_MP_LIMB);

  count_leading_zeros (normalization_steps, dp[dsize - 1]);

  /* Normalize the denominator, i.e. make its most significant bit set by
     shifting it NORMALIZATION_STEPS bits to the left.  Also shift the
     numerator the same number of steps (to keep the quotient the same!).  */
  if (normalization_steps != 0)
    {
      mp_ptr tp;
      mp_limb_t nlimb;

      /* Shift up the denominator setting the most significant bit of
	 the most significant limb.  Use temporary storage not to clobber
	 the original contents of the denominator.  */
      tp = (mp_ptr) TMP_ALLOC (dsize * BYTES_PER_MP_LIMB);
      mpn_lshift (tp, dp, dsize, normalization_steps);
      dp = tp;

      if (rsize > nsize)
	{
	  MPN_ZERO (rp, rsize - nsize);
	  nlimb = mpn_lshift (rp + (rsize - nsize),
			      np, nsize, normalization_steps);
	}
      else
	{
	  nlimb = mpn_lshift (rp, np + (nsize - rsize),
			      rsize, normalization_steps);
	}
      if (nlimb != 0)
	{
	  rp[rsize] = nlimb;
	  rsize++;
	}
    }
  else
    {
      if (rsize > nsize)
	{
	  MPN_ZERO (rp, rsize - nsize);
	  MPN_COPY (rp + (rsize - nsize), np, nsize);
	}
      else
	{
	  MPN_COPY (rp, np + (nsize - rsize), rsize);
	}
    }

  qlimb = mpn_divmod (qp, rp, rsize, dp, dsize);
  qsize = rsize - dsize;
  if (qlimb)
    {
      qp[qsize] = qlimb;
      qsize++;
    }

  {
    double res;
    mp_size_t i;

    res = qp[qsize - 1];
    for (i = qsize - 2; i >= 0; i--)
      res = res * MP_BASE_AS_DOUBLE + qp[i];

    res = __gmp_scale2 (res, BITS_PER_MP_LIMB * (nsize - dsize - N_QLIMBS));

    TMP_FREE (marker);
    return sign_quotient >= 0 ? res : -res;
  }
}
