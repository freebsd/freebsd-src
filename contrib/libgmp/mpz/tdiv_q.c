/* mpz_tdiv_q -- divide two integers and produce a quotient.

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
mpz_tdiv_q (mpz_ptr quot, mpz_srcptr num, mpz_srcptr den)
#else
mpz_tdiv_q (quot, num, den)
     mpz_ptr quot;
     mpz_srcptr num;
     mpz_srcptr den;
#endif
{
  mp_srcptr np, dp;
  mp_ptr qp, rp;
  mp_size_t nsize = num->_mp_size;
  mp_size_t dsize = den->_mp_size;
  mp_size_t qsize, rsize;
  mp_size_t sign_quotient = nsize ^ dsize;
  unsigned normalization_steps;
  mp_limb_t q_limb;
  TMP_DECL (marker);

  nsize = ABS (nsize);
  dsize = ABS (dsize);

  /* Ensure space is enough for quotient. */

  qsize = nsize - dsize + 1;	/* qsize cannot be bigger than this.  */
  if (qsize <= 0)
    {
      quot->_mp_size = 0;
      return;
    }

  if (quot->_mp_alloc < qsize)
    _mpz_realloc (quot, qsize);

  qp = quot->_mp_d;
  np = num->_mp_d;
  dp = den->_mp_d;

  /* Optimize division by a single-limb divisor.  */
  if (dsize == 1)
    {
      mpn_divmod_1 (qp, np, nsize, dp[0]);
      qsize -= qp[qsize - 1] == 0;
      quot->_mp_size = sign_quotient >= 0 ? qsize : -qsize;
      return;
    }

  TMP_MARK (marker);

  rp = (mp_ptr) TMP_ALLOC ((nsize + 1) * BYTES_PER_MP_LIMB);

  count_leading_zeros (normalization_steps, dp[dsize - 1]);

  /* Normalize the denominator, i.e. make its most significant bit set by
     shifting it NORMALIZATION_STEPS bits to the left.  Also shift the
     numerator the same number of steps (to keep the quotient the same!).  */
  if (normalization_steps != 0)
    {
      mp_ptr tp;
      mp_limb_t nlimb;

      /* Shift up the denominator setting the most significant bit of
	 the most significant word.  Use temporary storage not to clobber
	 the original contents of the denominator.  */
      tp = (mp_ptr) TMP_ALLOC (dsize * BYTES_PER_MP_LIMB);
      mpn_lshift (tp, dp, dsize, normalization_steps);
      dp = tp;

      /* Shift up the numerator, possibly introducing a new most
	 significant word.  Move the shifted numerator in the remainder
	 meanwhile.  */
      nlimb = mpn_lshift (rp, np, nsize, normalization_steps);
      if (nlimb != 0)
	{
	  rp[nsize] = nlimb;
	  rsize = nsize + 1;
	}
      else
	rsize = nsize;
    }
  else
    {
      /* The denominator is already normalized, as required.  Copy it to
	 temporary space if it overlaps with the quotient.  */
      if (dp == qp)
	{
	  dp = (mp_ptr) TMP_ALLOC (dsize * BYTES_PER_MP_LIMB);
	  MPN_COPY ((mp_ptr) dp, qp, dsize);
	}

      /* Move the numerator to the remainder.  */
      MPN_COPY (rp, np, nsize);
      rsize = nsize;
    }

  q_limb = mpn_divmod (qp, rp, rsize, dp, dsize);

  qsize = rsize - dsize;
  if (q_limb)
    {
      qp[qsize] = q_limb;
      qsize += 1;
    }

  quot->_mp_size = sign_quotient >= 0 ? qsize : -qsize;
  TMP_FREE (marker);
}
