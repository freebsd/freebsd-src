/* mpz_dmincl.c -- include file for mpz_dm.c, mpz_mod.c, mdiv.c.

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

/* THIS CODE IS OBSOLETE.  IT WILL SOON BE REPLACED BY CLEANER CODE WITH
   LESS MEMORY ALLOCATION OVERHEAD.  */

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
  mp_size nsize = num->size;
  mp_size dsize = den->size;
  mp_size qsize, rsize;
  mp_size sign_remainder = nsize;
#ifdef COMPUTE_QUOTIENT
  mp_size sign_quotient = nsize ^ dsize;
#endif
  unsigned normalization_steps;

  nsize = ABS (nsize);
  dsize = ABS (dsize);

  /* Ensure space is enough for quotient and remainder. */

  /* We need space for an extra limb in the remainder, because it's
     up-shifted (normalized) below.  */
  rsize = nsize + 1;
  if (rem->alloc < rsize)
    _mpz_realloc (rem, rsize);

  qsize = nsize - dsize + 1;	/* qsize cannot be bigger than this.  */
  if (qsize <= 0)
    {
#ifdef COMPUTE_QUOTIENT
      quot->size = 0;
#endif
      if (num != rem)
	{
	  rem->size = num->size;
	  MPN_COPY (rem->d, num->d, nsize);
	}
      return;
    }

#ifdef COMPUTE_QUOTIENT
  if (quot->alloc < qsize)
    _mpz_realloc (quot, qsize);
  qp = quot->d;
#else
  qp = (mp_ptr) alloca (qsize * BYTES_PER_MP_LIMB);
#endif
  np = num->d;
  dp = den->d;
  rp = rem->d;

  /* Make sure quot and num are different.  Otherwise the numerator
     would be successively overwritten by the quotient digits.  */
  if (qp == np)
    {
      np = (mp_ptr) alloca (nsize * BYTES_PER_MP_LIMB);
      MPN_COPY (np, qp, nsize);
    }

  count_leading_zeros (normalization_steps, dp[dsize - 1]);

  /* Normalize the denominator, i.e. make its most significant bit set by
     shifting it NORMALIZATION_STEPS bits to the left.  Also shift the
     numerator the same number of steps (to keep the quotient the same!).  */
  if (normalization_steps != 0)
    {
      mp_ptr tp;
      mp_limb ndigit;

      /* Shift up the denominator setting the most significant bit of
	 the most significant word.  Use temporary storage not to clobber
	 the original contents of the denominator.  */
      tp = (mp_ptr) alloca (dsize * BYTES_PER_MP_LIMB);
      (void) mpn_lshift (tp, dp, dsize, normalization_steps);
      dp = tp;

      /* Shift up the numerator, possibly introducing a new most
	 significant word.  Move the shifted numerator in the remainder
	 meanwhile.  */
      ndigit = mpn_lshift (rp, np, nsize, normalization_steps);
      if (ndigit != 0)
	{
	  rp[nsize] = ndigit;
	  rsize = nsize + 1;
	}
      else
	rsize = nsize;
    }
  else
    {
#ifdef COMPUTE_QUOTIENT
      if (rem == den || quot == den)
#else
      if (rem == den)
#endif
	{
	  mp_ptr tp;

	  tp = (mp_ptr) alloca (dsize * BYTES_PER_MP_LIMB);
	  MPN_COPY (tp, dp, dsize);
	  dp = tp;
	}

      /* Move the numerator to the remainder.  */
      if (rp != np)
	MPN_COPY (rp, np, nsize);

      rsize = nsize;
    }

  qsize = rsize - dsize + mpn_div (qp, rp, rsize, dp, dsize);

  rsize = dsize;

  /* Normalize the remainder.  */
  while (rsize > 0)
    {
      if (rp[rsize - 1] != 0)
	break;
      rsize--;
    }

  if (normalization_steps != 0)
    rsize = mpn_rshift (rp, rp, rsize, normalization_steps);

  rem->size = (sign_remainder >= 0) ? rsize : -rsize;

#ifdef COMPUTE_QUOTIENT
  /* Normalize the quotient.  We may have at most one leading
     zero-word, so no loop is needed.  */
  if (qsize > 0)
    qsize -= (qp[qsize - 1] == 0);

  quot->size = (sign_quotient >= 0) ? qsize : -qsize;
#endif

  alloca (0);
}
