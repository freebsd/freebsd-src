/* dmincl.c -- include file for tdiv_qr.c, tdiv_r.c.

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

/* If den == quot, den needs temporary storage.
   If den == rem, den needs temporary storage.
   If num == quot, num needs temporary storage.
   If den has temporary storage, it can be normalized while being copied,
     i.e no extra storage should be allocated.  */

/* This is the function body of mdiv, mpz_divmod, and mpz_mod.

   If COMPUTE_QUOTIENT is defined, the quotient is put in the MP_INT
   object quot, otherwise that variable is not referenced at all.

   The remainder is always computed, and the result is put in the MP_INT
   object rem.  */

{
  mp_ptr np, dp;
  mp_ptr qp, rp;
  mp_size_t nsize = num->_mp_size;
  mp_size_t dsize = den->_mp_size;
  mp_size_t qsize, rsize;
  mp_size_t sign_remainder = nsize;
#ifdef COMPUTE_QUOTIENT
  mp_size_t sign_quotient = nsize ^ dsize;
#endif
  unsigned normalization_steps;
  mp_limb_t q_limb;
  TMP_DECL (marker);

  nsize = ABS (nsize);
  dsize = ABS (dsize);

  /* Ensure space is enough for quotient and remainder. */

  /* We need space for an extra limb in the remainder, because it's
     up-shifted (normalized) below.  */
  rsize = nsize + 1;
  if (rem->_mp_alloc < rsize)
    _mpz_realloc (rem, rsize);

  qsize = rsize - dsize;	/* qsize cannot be bigger than this.  */
  if (qsize <= 0)
    {
      if (num != rem)
	{
	  rem->_mp_size = num->_mp_size;
	  MPN_COPY (rem->_mp_d, num->_mp_d, nsize);
	}
#ifdef COMPUTE_QUOTIENT
      /* This needs to follow the assignment to rem, in case the
	 numerator and quotient are the same.  */
      quot->_mp_size = 0;
#endif
      return;
    }

#ifdef COMPUTE_QUOTIENT
  if (quot->_mp_alloc < qsize)
    _mpz_realloc (quot, qsize);
#endif

  /* Read pointers here, when reallocation is finished.  */
  np = num->_mp_d;
  dp = den->_mp_d;
  rp = rem->_mp_d;

  /* Optimize division by a single-limb divisor.  */
  if (dsize == 1)
    {
      mp_limb_t rlimb;
#ifdef COMPUTE_QUOTIENT
      qp = quot->_mp_d;
      rlimb = mpn_divmod_1 (qp, np, nsize, dp[0]);
      qsize -= qp[qsize - 1] == 0;
      quot->_mp_size = sign_quotient >= 0 ? qsize : -qsize;
#else
      rlimb = mpn_mod_1 (np, nsize, dp[0]);
#endif
      rp[0] = rlimb;
      rsize = rlimb != 0;
      rem->_mp_size = sign_remainder >= 0 ? rsize : -rsize;
      return;
    }

  TMP_MARK (marker);

#ifdef COMPUTE_QUOTIENT
  qp = quot->_mp_d;

  /* Make sure QP and NP point to different objects.  Otherwise the
     numerator would be gradually overwritten by the quotient limbs.  */
  if (qp == np)
    {
      /* Copy NP object to temporary space.  */
      np = (mp_ptr) TMP_ALLOC (nsize * BYTES_PER_MP_LIMB);
      MPN_COPY (np, qp, nsize);
    }

#else
  /* Put quotient at top of remainder. */
  qp = rp + dsize;
#endif

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
	 temporary space if it overlaps with the quotient or remainder.  */
#ifdef COMPUTE_QUOTIENT
      if (dp == rp || dp == qp)
#else
      if (dp == rp)
#endif
	{
	  mp_ptr tp;

	  tp = (mp_ptr) TMP_ALLOC (dsize * BYTES_PER_MP_LIMB);
	  MPN_COPY (tp, dp, dsize);
	  dp = tp;
	}

      /* Move the numerator to the remainder.  */
      if (rp != np)
	MPN_COPY (rp, np, nsize);

      rsize = nsize;
    }

  q_limb = mpn_divmod (qp, rp, rsize, dp, dsize);

#ifdef COMPUTE_QUOTIENT
  qsize = rsize - dsize;
  if (q_limb)
    {
      qp[qsize] = q_limb;
      qsize += 1;
    }

  quot->_mp_size = sign_quotient >= 0 ? qsize : -qsize;
#endif

  rsize = dsize;
  MPN_NORMALIZE (rp, rsize);

  if (normalization_steps != 0 && rsize != 0)
    {
      mpn_rshift (rp, rp, rsize, normalization_steps);
      rsize -= rp[rsize - 1] == 0;
    }

  rem->_mp_size = sign_remainder >= 0 ? rsize : -rsize;
  TMP_FREE (marker);
}
