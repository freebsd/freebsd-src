/* mpf_set_str (dest, string, base) -- Convert the string STRING
   in base BASE to a float in dest.  If BASE is zero, the leading characters
   of STRING is used to figure out the base.

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

#include <string.h>
#include <ctype.h>
#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

long int strtol _PROTO ((const char *, char **ptr, int));

static int
digit_value_in_base (c, base)
     int c;
     int base;
{
  int digit;

  if (isdigit (c))
    digit = c - '0';
  else if (islower (c))
    digit = c - 'a' + 10;
  else if (isupper (c))
    digit = c - 'A' + 10;
  else
    return -1;

  if (digit < base)
    return digit;
  return -1;
}

int
#if __STDC__
mpf_set_str (mpf_ptr x, const char *str, int base)
#else
mpf_set_str (x, str, base)
     mpf_ptr x;
     char *str;
     int base;
#endif
{
  size_t str_size;
  char *s, *begs;
  size_t i;
  mp_size_t xsize;
  int c;
  int negative;
  char *dotpos = 0;
  int expflag;
  int decimal_exponent_flag;
  TMP_DECL (marker);

  TMP_MARK (marker);

  c = *str;

  /* Skip whitespace.  */
  while (isspace (c))
    c = *++str;

  negative = 0;
  if (c == '-')
    {
      negative = 1;
      c = *++str;
    }

  decimal_exponent_flag = base < 0;
  base = ABS (base);

  if (digit_value_in_base (c, base == 0 ? 10 : base) < 0)
    return -1;			/* error if no digits */

  /* If BASE is 0, try to find out the base by looking at the initial
     characters.  */
  if (base == 0)
    {
      base = 10;
#if 0
      if (c == '0')
	{
	  base = 8;
	  c = *++str;
	  if (c == 'x' || c == 'X')
	    base = 16;
	}
#endif
    }

  expflag = 0;
  str_size = strlen (str);
  for (i = 0; i < str_size; i++)
    {
      c = str[i];
      if (c == '@' || (base <= 10 && (c == 'e' || c == 'E')))
	{
	  expflag = 1;
	  str_size = i;
	  break;
	}

    }

  s = begs = (char *) TMP_ALLOC (str_size + 1);

  for (i = 0; i < str_size; i++)
    {
      c = *str;
      if (!isspace (c))
	{
	  int dig;

	  if (c == '.')
	    {
	      if (dotpos != 0)
		{
		  TMP_FREE (marker);
		  return -1;
		}
	      dotpos = s;
	    }
	  else
	    {
	      dig = digit_value_in_base (c, base);
	      if (dig < 0)
		{
		  TMP_FREE (marker);
		  return -1;
		}
	      *s++ = dig;
	    }
	}
      c = *++str;
    }

  str_size = s - begs;

  xsize = str_size / __mp_bases[base].chars_per_limb + 2;
  {
    long exp_in_base;
    mp_size_t rsize, msize;
    int cnt, i;
    mp_ptr mp, xp, tp, rp;
    mp_limb_t cy;
    mp_exp_t exp_in_limbs;
    mp_size_t prec = x->_mp_prec;
    int divflag;
    mp_size_t xxx = 0;

    mp = (mp_ptr) TMP_ALLOC (xsize * BYTES_PER_MP_LIMB);
    msize = mpn_set_str (mp, (unsigned char *) begs, str_size, base);

    if (msize == 0)
      {
	x->_mp_size = 0;
	x->_mp_exp = 0;
	TMP_FREE (marker);
	return 0;
      }

    if (expflag != 0)
      exp_in_base = strtol (str + 1, (char **) 0,
			    decimal_exponent_flag ? 10 : base);
    else
      exp_in_base = 0;
    if (dotpos != 0)
      exp_in_base -= s - dotpos;
    divflag = exp_in_base < 0;
    exp_in_base = ABS (exp_in_base);

    if (exp_in_base == 0)
      {
	MPN_COPY (x->_mp_d, mp, msize);
	x->_mp_size = negative ? -msize : msize;
	x->_mp_exp = msize;
	TMP_FREE (marker);
	return 0;
      }

#if 1
    rsize = (((mp_size_t) (exp_in_base / __mp_bases[base].chars_per_bit_exactly))
	     / BITS_PER_MP_LIMB + 3);
#else
    count_leading_zeros (cnt, (mp_limb_t) base);
    rsize = exp_in_base - cnt * exp_in_base / BITS_PER_MP_LIMB + 1;
#endif
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

    if (rsize > prec)
      {
	xxx += rsize - prec;
	rp += rsize - prec;
	rsize = prec;
      }
#if 0
    if (msize > prec)
      {
	xxx -= msize - prec;
	mp += msize - prec;
	msize = prec;
      }
#endif
    if (divflag)
      {
	mp_ptr qp;
	mp_limb_t qflag;
	mp_size_t xtra;
	if (msize <= rsize)
	  {
	    /* Allocate extra limb for current divrem sematics. */
	    mp_ptr tmp = (mp_ptr) TMP_ALLOC ((rsize + 1) * BYTES_PER_MP_LIMB);
	    MPN_ZERO (tmp, rsize - msize);
	    MPN_COPY (tmp + rsize - msize, mp, msize);
	    mp = tmp;
	    xxx += rsize - msize;
	    msize = rsize;
	  }
	count_leading_zeros (cnt, rp[rsize - 1]);
	if (cnt != 0)
	  {
	    mpn_lshift (rp, rp, rsize, cnt);
	    cy = mpn_lshift (mp, mp, msize, cnt);
	    if (cy)
	      mp[msize++] = cy;
	  }
	qp = (mp_ptr) TMP_ALLOC ((prec + 1) * BYTES_PER_MP_LIMB);
	xtra = prec - (msize - rsize);
	qflag = mpn_divrem (qp, xtra, mp, msize, rp, rsize);
	qp[prec] = qflag;
	tp = qp;
	rsize = prec + qflag;
	exp_in_limbs = rsize - xtra - xxx;
      }
    else
      {
	tp = (mp_ptr) TMP_ALLOC ((rsize + msize) * BYTES_PER_MP_LIMB);
	if (rsize > msize)
	  mpn_mul (tp, rp, rsize, mp, msize);
	else
	  mpn_mul (tp, mp, msize, rp, rsize);
	rsize += msize;
	rsize -= tp[rsize - 1] == 0;
	exp_in_limbs = rsize + xxx;

	if (rsize > prec)
	  {
	    xxx = rsize - prec;
	    tp += rsize - prec;
	    rsize = prec;
	    exp_in_limbs += 0;
	  }
      }

    MPN_COPY (x->_mp_d, tp, rsize);
    x->_mp_size = negative ? -rsize : rsize;
    x->_mp_exp = exp_in_limbs;
    TMP_FREE (marker);
    return 0;
  }
}
