/* _mpz_get_str (string, base, mp_src) -- Convert the multiple precision
   number MP_SRC to a string STRING of base BASE.  If STRING is NULL
   allocate space for the result.  In any case, return a pointer to the
   result.  If STRING is not NULL, the caller must ensure enough space is
   available to store the result.

Copyright (C) 1991, 1993 Free Software Foundation, Inc.

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

#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

#ifndef UMUL_TIME
#define UMUL_TIME 1
#endif

#ifndef UDIV_TIME
#define UDIV_TIME UMUL_TIME
#endif

#define udiv_qrnndx(q, r, nh, nl, d, di) \
  do {									\
    unsigned long int _q, _ql, _r;					\
    unsigned long int _xh, _xl;						\
    umul_ppmm (_q, _ql, (nh), (di));					\
    _q += (nh);			/* DI is 2**32 too small.  Compensate */\
    if (_q < (nh))							\
      {									\
	/* Got carry.  Propagate it in the multiplication.  */		\
	umul_ppmm (_xh, _xl, (d), _q);					\
	_xh += (d);							\
      }									\
    else								\
      umul_ppmm (_xh, _xl, (d), _q);					\
    sub_ddmmss (_xh, _r, (nh), (nl), _xh, _xl);				\
    if (_xh != 0)							\
      {									\
	sub_ddmmss (_xh, _r, _xh, _r, 0, (d));				\
	_q += 1;							\
	if (_xh != 0)							\
	  {								\
	    sub_ddmmss (_xh, _r, _xh, _r, 0, (d));			\
	    _q += 1;							\
	  }								\
      }									\
    if (_r >= (d))							\
      {									\
	_r -= (d);							\
	_q += 1;							\
      }									\
    (r) = _r;								\
    (q) = _q;								\
  } while (0)

char *
#ifdef __STDC__
_mpz_get_str (char *str, int base, const MP_INT *m)
#else
_mpz_get_str (str, base, m)
     char *str;
     int base;
     const MP_INT *m;
#endif
{
  mp_ptr tp;
  mp_size msize;
  mp_limb big_base;
#if UDIV_NEEDS_NORMALIZATION || UDIV_TIME > 2 * UMUL_TIME

  int normalization_steps;
#if UDIV_TIME > 2 * UMUL_TIME
  mp_limb big_base_inverted;
#endif
#endif
  unsigned int dig_per_u;
  mp_size out_len;
  char *s;
  char *num_to_ascii;

  if (base >= 0)
    {
      if (base == 0)
	base = 10;
      num_to_ascii = "0123456789abcdefghijklmnopqrstuvwxyz";
    }
  else
    {
      base = -base;
      num_to_ascii = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    }

  dig_per_u = __mp_bases[base].chars_per_limb;
  out_len = mpz_sizeinbase (m, base) + 1;
  big_base = __mp_bases[base].big_base;

  msize = m->size;

  if (str == NULL)
    str = (char *) (*_mp_allocate_func) (out_len + (msize < 0));

  if (msize < 0)
    *str++ = '-';
  s = str;

  msize = ABS (msize);

  /* Special case zero, as the code below doesn't handle it.  */
  if (msize == 0)
    {
      s[0] = '0';
      s[1] = 0;
      return str;
    }

  if ((base & (base - 1)) == 0)
    {
      /* The base is a power of 2.  Make conversion from most
	 significant side.  */
      mp_limb n1, n0;
      int bits_per_digit = big_base;
      int x;
      int bit_pos;
      int i;
      unsigned mask = (1 << bits_per_digit) - 1;

      tp = m->d;
      n1 = tp[msize - 1];
      count_leading_zeros (x, n1);

	/* BIT_POS should be R when input ends in least sign. nibble,
	   R + bits_per_digit * n when input ends in n:th least significant
	   nibble. */

      {
	int bits;

	bits = BITS_PER_MP_LIMB * msize - x;
	x = bits % bits_per_digit;
	if (x != 0)
	  bits += bits_per_digit - x;
	bit_pos = bits - (msize - 1) * BITS_PER_MP_LIMB;
      }

      /* Fast loop for bit output.  */
      i = msize - 1;
      for (;;)
	{
	  bit_pos -= bits_per_digit;
	  while (bit_pos >= 0)
	    {
	      *s++ = num_to_ascii[(n1 >> bit_pos) & mask];
	      bit_pos -= bits_per_digit;
	    }
	  i--;
	  if (i < 0)
	    break;
	  n0 = (n1 << -bit_pos) & mask;
	  n1 = tp[i];
	  bit_pos += BITS_PER_MP_LIMB;
	  *s++ = num_to_ascii[n0 | (n1 >> bit_pos)];
	}

      *s = 0;
    }
  else
    {
      /* General case.  The base is not a power of 2.  Make conversion
	 from least significant end.  */

      /* If udiv_qrnnd only handles divisors with the most significant bit
	 set, prepare BIG_BASE for being a divisor by shifting it to the
	 left exactly enough to set the most significant bit.  */
#if UDIV_NEEDS_NORMALIZATION || UDIV_TIME > 2 * UMUL_TIME
      count_leading_zeros (normalization_steps, big_base);
      big_base <<= normalization_steps;
#if UDIV_TIME > 2 * UMUL_TIME
      /* Get the fixed-point approximation to 1/BIG_BASE.  */
      big_base_inverted = __mp_bases[base].big_base_inverted;
#endif
#endif

      out_len--;		/* now not include terminating \0 */
      s += out_len;

      /* Allocate temporary space and move the multi prec number to
	 convert there, as we need to overwrite it below, while
	 computing the successive remainders.  */
      tp = (mp_ptr) alloca ((msize + 1) * BYTES_PER_MP_LIMB);
      MPN_COPY (tp, m->d, msize);

      while (msize != 0)
	{
	  int i;
	  mp_limb n0, n1;

#if UDIV_NEEDS_NORMALIZATION || UDIV_TIME > 2 * UMUL_TIME
	  /* If we shifted BIG_BASE above, shift the dividend too, to get
	     the right quotient.  We need to do this every loop,
	     as the intermediate quotients are OK, but the quotient from
	     one turn in the loop is going to be the dividend in the
	     next turn, and the dividend needs to be up-shifted.  */
	  if (normalization_steps != 0)
	    {
	      n0 = mpn_lshift (tp, tp, msize, normalization_steps);

	      /* If the shifting gave a carry out limb, store it and
		 increase the length.  */
	      if (n0 != 0)
		{
		  tp[msize] = n0;
		  msize++;
		}
	    }
#endif

	  /* Divide the number at TP with BIG_BASE to get a quotient and a
	     remainder.  The remainder is our new digit in base BIG_BASE.  */
	  i = msize - 1;
	  n1 = tp[i];

	  if (n1 >= big_base)
	    n1 = 0;
	  else
	    {
	      msize--;
	      i--;
	    }

	  for (; i >= 0; i--)
	    {
	      n0 = tp[i];
#if UDIV_TIME > 2 * UMUL_TIME
	      udiv_qrnndx (tp[i], n1, n1, n0, big_base, big_base_inverted);
#else
	      udiv_qrnnd (tp[i], n1, n1, n0, big_base);
#endif
	    }

#if UDIV_NEEDS_NORMALIZATION || UDIV_TIME > 2 * UMUL_TIME
	  /* If we shifted above (at previous UDIV_NEEDS_NORMALIZATION tests)
	     the remainder will be up-shifted here.  Compensate.  */
	  n1 >>= normalization_steps;
#endif

	  /* Convert N1 from BIG_BASE to a string of digits in BASE
	     using single precision operations.  */
	  for (i = dig_per_u - 1; i >= 0; i--)
	    {
	      *--s = num_to_ascii[n1 % base];
	      n1 /= base;
	      /* Break from the loop as soon as we would only write zeros.  */
	      if (n1 == 0 && msize == 0)
		break;
	    }
	}

      /* There should be no leading zeros.  */
      if (*s == '0')
	abort ();

      if (s == str)
	{
	  /* This should be the common case.  */
	  s[out_len] = 0;
	}
      else if (s == str + 1)
	{
	  /* The string became 1 digit shorter than its maximum.  */
	  /* Need to copy it back one char pos.  */
	  out_len--;
#ifndef HAS_MEMMOVE
	  {
	    size_t i;

	    for (i = 0; i < out_len; i++)
	      str[i] = s[i];
	  }
#else
	  memmove (str, s, out_len);
#endif
	  str[out_len] = 0;
	}
      else
	{
	  /* Hopefully never.  */
	  abort ();
	}
    }

  alloca (0);
  /* Ugly, we incremented str for negative numbers.  Fix that here.  */
  return str - (m->size < 0);
}
