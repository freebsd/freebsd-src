/* mpf_get_str (digit_ptr, exp, base, n_digits, a) -- Convert the floating
  point number A to a base BASE number and store N_DIGITS raw digits at
  DIGIT_PTR, and the base BASE exponent in the word pointed to by EXP.  For
  example, the number 3.1416 would be returned as "31416" in DIGIT_PTR and
  1 in EXP.

Copyright (C) 1993, 1994, 1995, 1996 Free Software Foundation, Inc.

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

/*
   New algorithm for converting fractions (951019):
   0. Call the fraction to convert F.
   1. Compute [exp * log(2^BITS_PER_MP_LIMB)/log(B)], i.e.,
      [exp * BITS_PER_MP_LIMB * __mp_bases[B].chars_per_bit_exactly].  Exp is
      the number of limbs between the limb point and the most significant
      non-zero limb.  Call this result n.
   2. Compute B^n.
   3. F*B^n will now be just below 1, which can be converted easily.  (Just
      multiply by B repeatedly, and see the digits fall out as integers.)
   We should interrupt the conversion process of F*B^n as soon as the number
   of digits requested have been generated.

   New algorithm for converting integers (951019):
   0. Call the integer to convert I.
   1. Compute [exp * log(2^BITS_PER_MP_LIMB)/log(B)], i.e.,
      [exp BITS_PER_MP_LIMB * __mp_bases[B].chars_per_bit_exactly].  Exp is
      the number of limbs between the limb point and the least significant
      non-zero limb.  Call this result n.
   2. Compute B^n.
   3. I/B^n can be converted easily.  (Just divide by B repeatedly.  In GMP,
      this is best done by calling mpn_get_str.)
   Note that converting I/B^n could yield more digits than requested.  For
   efficiency, the variable n above should be set larger in such cases, to
   kill all undesired digits in the division in step 3.
*/

char *
#if __STDC__
mpf_get_str (char *digit_ptr, mp_exp_t *exp, int base, size_t n_digits, mpf_srcptr u)
#else
mpf_get_str (digit_ptr, exp, base, n_digits, u)
     char *digit_ptr;
     mp_exp_t *exp;
     int base;
     size_t n_digits;
     mpf_srcptr u;
#endif
{
  mp_size_t usize;
  mp_exp_t uexp;
  unsigned char *str;
  size_t str_size;
  char *num_to_text;
  long i;			/* should be size_t */
  mp_ptr rp;
  mp_limb_t big_base;
  size_t digits_computed_so_far;
  int dig_per_u;
  mp_srcptr up;
  unsigned char *tstr;
  mp_exp_t exp_in_base;
  TMP_DECL (marker);

  TMP_MARK (marker);
  usize = u->_mp_size;
  uexp = u->_mp_exp;

  if (base >= 0)
    {
      if (base == 0)
	base = 10;
      num_to_text = "0123456789abcdefghijklmnopqrstuvwxyz";
    }
  else
    {
      base = -base;
      num_to_text = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    }

  /* Don't compute more digits than U can accurately represent.
     Also, if 0 digits were requested, give *exactly* as many digits
     as can be accurately represented.  */
  {
    size_t max_digits = (((u->_mp_prec - 1) * BITS_PER_MP_LIMB)
			 * __mp_bases[base].chars_per_bit_exactly);
    if (n_digits == 0 || n_digits > max_digits)
      n_digits = max_digits;
  }

  if (digit_ptr == 0)
    {
      /* We didn't get a string from the user.  Allocate one (and return
	 a pointer to it) with space for `-' and terminating null.  */
      digit_ptr = (char *) (*_mp_allocate_func) (n_digits + 2);
    }

  if (usize == 0)
    {
      *exp = 0;
      *digit_ptr = 0;
      return digit_ptr;
    }

  str = (unsigned char *) digit_ptr;

  /* Allocate temporary digit space.  We can't put digits directly in the user
     area, since we almost always generate more digits than requested.  */
  tstr = (unsigned char *) TMP_ALLOC (n_digits + 3 * BITS_PER_MP_LIMB);

  if (usize < 0)
    {
      *digit_ptr = '-';
      str++;
      usize = -usize;
    }

  digits_computed_so_far = 0;

  if (uexp > usize)
    {
      /* The number has just an integral part.  */
      mp_size_t rsize;
      mp_size_t exp_in_limbs;
      mp_size_t msize;
      mp_ptr tp, xp, mp;
      int cnt;
      mp_limb_t cy;
      mp_size_t start_str;
      mp_size_t n_limbs;

      n_limbs = 2 + ((mp_size_t) (n_digits / __mp_bases[base].chars_per_bit_exactly)
		     / BITS_PER_MP_LIMB);

      /* Compute n such that [u/B^n] contains (somewhat) more than n_digits
	 digits.  (We compute less than that only if that is an exact number,
	 i.e., exp is small enough.)  */

      exp_in_limbs = uexp;

      if (n_limbs >= exp_in_limbs)
	{
	  /* The number is so small that we convert the entire number.  */
	  exp_in_base = 0;
	  rp = (mp_ptr) TMP_ALLOC (exp_in_limbs * BYTES_PER_MP_LIMB);
	  MPN_ZERO (rp, exp_in_limbs - usize);
	  MPN_COPY (rp + (exp_in_limbs - usize), u->_mp_d, usize);
	  rsize = exp_in_limbs;
	}
      else
	{
	  exp_in_limbs -= n_limbs;
	  exp_in_base = (((exp_in_limbs * BITS_PER_MP_LIMB - 1))
			 * __mp_bases[base].chars_per_bit_exactly);

	  rsize = exp_in_limbs + 1;
	  rp = (mp_ptr) TMP_ALLOC (rsize * BYTES_PER_MP_LIMB);
	  tp = (mp_ptr) TMP_ALLOC (rsize * BYTES_PER_MP_LIMB);

	  rp[0] = base;
	  rsize = 1;

	  count_leading_zeros (cnt, exp_in_base);
	  for (i = BITS_PER_MP_LIMB - cnt - 2; i >= 0; i--)
	    {
	      mpn_mul_n (tp, rp, rp, rsize);
	      rsize = 2 * rsize;
	      rsize -= tp[rsize - 1] == 0;
	      xp = tp; tp = rp; rp = xp;

	      if (((exp_in_base >> i) & 1) != 0)
		{
		  cy = mpn_mul_1 (rp, rp, rsize, (mp_limb_t) base);
		  rp[rsize] = cy;
		  rsize += cy != 0;
		}
	    }

	  mp = u->_mp_d;
	  msize = usize;

	  {
	    mp_ptr qp;
	    mp_limb_t qflag;
	    mp_size_t xtra;
	    if (msize < rsize)
	      {
		mp_ptr tmp = (mp_ptr) TMP_ALLOC ((rsize+1)* BYTES_PER_MP_LIMB);
		MPN_ZERO (tmp, rsize - msize);
		MPN_COPY (tmp + rsize - msize, mp, msize);
		mp = tmp;
		msize = rsize;
	      }
	    else
	      {
		mp_ptr tmp = (mp_ptr) TMP_ALLOC ((msize+1)* BYTES_PER_MP_LIMB);
		MPN_COPY (tmp, mp, msize);
		mp = tmp;
	      }
	    count_leading_zeros (cnt, rp[rsize - 1]);
	    cy = 0;
	    if (cnt != 0)
	      {
		mpn_lshift (rp, rp, rsize, cnt);
		cy = mpn_lshift (mp, mp, msize, cnt);
		if (cy)
		  mp[msize++] = cy;
	      }

	    {
	      mp_size_t qsize = n_limbs + (cy != 0);
	      qp = (mp_ptr) TMP_ALLOC ((qsize + 1) * BYTES_PER_MP_LIMB);
	      xtra = qsize - (msize - rsize);
	      qflag = mpn_divrem (qp, xtra, mp, msize, rp, rsize);
	      qp[qsize] = qflag;
	      rsize = qsize + qflag;
	      rp = qp;
	    }
	  }
	}

      str_size = mpn_get_str (tstr, base, rp, rsize);

      if (str_size > n_digits + 3 * BITS_PER_MP_LIMB)
	abort ();

      start_str = 0;
      while (tstr[start_str] == 0)
	start_str++;

      for (i = start_str; i < str_size; i++)
	{
	  tstr[digits_computed_so_far++] = tstr[i];
	  if (digits_computed_so_far > n_digits)
	    break;
	}
      exp_in_base = exp_in_base + str_size - start_str;
      goto finish_up;
    }

  exp_in_base = 0;

  if (uexp > 0)
    {
      /* The number has an integral part, convert that first.
	 If there is a fractional part too, it will be handled later.  */
      mp_size_t start_str;

      rp = (mp_ptr) TMP_ALLOC (uexp * BYTES_PER_MP_LIMB);
      up = u->_mp_d + usize - uexp;
      MPN_COPY (rp, up, uexp);

      str_size = mpn_get_str (tstr, base, rp, uexp);

      start_str = 0;
      while (tstr[start_str] == 0)
	start_str++;

      for (i = start_str; i < str_size; i++)
	{
	  tstr[digits_computed_so_far++] = tstr[i];
	  if (digits_computed_so_far > n_digits)
	    {
	      exp_in_base = str_size - start_str;
	      goto finish_up;
	    }
	}

      exp_in_base = str_size - start_str;
      /* Modify somewhat and fall out to convert fraction... */
      usize -= uexp;
      uexp = 0;
    }

  if (usize <= 0)
    goto finish_up;

  /* Convert the fraction.  */
  {
    mp_size_t rsize, msize;
    mp_ptr rp, tp, xp, mp;
    int cnt;
    mp_limb_t cy;
    mp_exp_t nexp;

    big_base = __mp_bases[base].big_base;
    dig_per_u = __mp_bases[base].chars_per_limb;

    /* Hack for correctly (although not efficiently) converting to bases that
       are powers of 2.  If we deem it important, we could handle powers of 2
       by shifting and masking (just like mpn_get_str).  */
    if (big_base < 10)		/* logarithm of base when power of two */
      {
	int logbase = big_base;
	if (dig_per_u * logbase == BITS_PER_MP_LIMB)
	  dig_per_u--;
	big_base = (mp_limb_t) 1 << (dig_per_u * logbase);
	/* fall out to general code... */
      }

#if 0
    if (0 && uexp == 0)
      {
	rp = (mp_ptr) TMP_ALLOC (usize * BYTES_PER_MP_LIMB);
	up = u->_mp_d;
	MPN_COPY (rp, up, usize);
	rsize = usize;
	nexp = 0;
      }
    else
      {}
#endif
    uexp = -uexp;
    if (u->_mp_d[usize - 1] == 0)
      cnt = 0;
    else
      count_leading_zeros (cnt, u->_mp_d[usize - 1]);

    nexp = ((uexp * BITS_PER_MP_LIMB) + cnt)
      * __mp_bases[base].chars_per_bit_exactly;

    if (nexp == 0)
      {
	rp = (mp_ptr) TMP_ALLOC (usize * BYTES_PER_MP_LIMB);
	up = u->_mp_d;
	MPN_COPY (rp, up, usize);
	rsize = usize;
      }
    else
      {
	rsize = uexp + 2;
	rp = (mp_ptr) TMP_ALLOC (rsize * BYTES_PER_MP_LIMB);
	tp = (mp_ptr) TMP_ALLOC (rsize * BYTES_PER_MP_LIMB);

	rp[0] = base;
	rsize = 1;

	count_leading_zeros (cnt, nexp);
	for (i = BITS_PER_MP_LIMB - cnt - 2; i >= 0; i--)
	  {
	    mpn_mul_n (tp, rp, rp, rsize);
	    rsize = 2 * rsize;
	    rsize -= tp[rsize - 1] == 0;
	    xp = tp; tp = rp; rp = xp;

	    if (((nexp >> i) & 1) != 0)
	      {
		cy = mpn_mul_1 (rp, rp, rsize, (mp_limb_t) base);
		rp[rsize] = cy;
		rsize += cy != 0;
	      }
	  }

	/* Did our multiplier (base^nexp) cancel with uexp?  */
#if 0
	if (uexp != rsize)
	  {
	    do
	      {
		cy = mpn_mul_1 (rp, rp, rsize, big_base);
		nexp += dig_per_u;
	      }
	    while (cy == 0);
	    rp[rsize++] = cy;
	  }
#endif
	mp = u->_mp_d;
	msize = usize;

	tp = (mp_ptr) TMP_ALLOC ((rsize + msize) * BYTES_PER_MP_LIMB);
	if (rsize > msize)
	  cy = mpn_mul (tp, rp, rsize, mp, msize);
	else
	  cy = mpn_mul (tp, mp, msize, rp, rsize);
	rsize += msize;
	rsize -= cy == 0;
	rp = tp;

	/* If we already output digits (for an integral part) pad
	   leading zeros.  */
	if (digits_computed_so_far != 0)
	  for (i = 0; i < nexp; i++)
	    tstr[digits_computed_so_far++] = 0;
      }

    while (digits_computed_so_far <= n_digits)
      {
	/* For speed: skip trailing zeroes.  */
	if (rp[0] == 0)
	  {
	    rp++;
	    rsize--;
	    if (rsize == 0)
	      {
		n_digits = digits_computed_so_far;
		break;
	      }
	  }

	cy = mpn_mul_1 (rp, rp, rsize, big_base);
	if (digits_computed_so_far == 0 && cy == 0)
	  {
	    abort ();
	    nexp += dig_per_u;
	    continue;
	  }
	/* Convert N1 from BIG_BASE to a string of digits in BASE
	   using single precision operations.  */
	{
	  unsigned char *s = tstr + digits_computed_so_far + dig_per_u;
	  for (i = dig_per_u - 1; i >= 0; i--)
	    {
	      *--s = cy % base;
	      cy /= base;
	    }
	}
	digits_computed_so_far += dig_per_u;
      }
    if (exp_in_base == 0)
      exp_in_base = -nexp;
  }

 finish_up:

  /* We can have at most one leading 0.  Remove it.  */
  if (tstr[0] == 0)
    {
      tstr++;
      digits_computed_so_far--;
      exp_in_base--;
    }

  /* We should normally have computed too many digits.  Round the result
     at the point indicated by n_digits.  */
  if (digits_computed_so_far > n_digits)
    {
      /* Round the result.  */
      if (tstr[n_digits] * 2 >= base)
	{
	  digits_computed_so_far = n_digits;
	  for (i = n_digits - 1; i >= 0; i--)
	    {
	      unsigned int x;
	      x = ++(tstr[i]);
	      if (x < base)
		goto rounded_ok;
	      digits_computed_so_far--;
	    }
	  tstr[0] = 1;
	  digits_computed_so_far = 1;
	  exp_in_base++;
	rounded_ok:;
	}
    }

  /* We might have fewer digits than requested as a result of rounding above,
     (i.e. 0.999999 => 1.0) or because we have a number that simply doesn't
     need many digits in this base (i.e., 0.125 in base 10).  */
  if (n_digits > digits_computed_so_far)
    n_digits = digits_computed_so_far;

  /* Remove trailing 0.  There can be many zeros. */
  while (n_digits != 0 && tstr[n_digits - 1] == 0)
    n_digits--;

  /* Translate to ascii and null-terminate.  */
  for (i = 0; i < n_digits; i++)
    *str++ = num_to_text[tstr[i]];
  *str = 0;
  *exp = exp_in_base;
  TMP_FREE (marker);
  return digit_ptr;
}

#if COPY_THIS_TO_OTHER_PLACES
      /* Use this expression in lots of places in the library instead of the
	 count_leading_zeros+expression that is used currently.  This expression
	 is much more accurate and will save odles of memory.  */
      rsize = ((mp_size_t) (exp_in_base / __mp_bases[base].chars_per_bit_exactly)
	       + BITS_PER_MP_LIMB) / BITS_PER_MP_LIMB;
#endif
