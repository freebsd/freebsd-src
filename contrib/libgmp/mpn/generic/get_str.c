/* mpn_get_str -- Convert a MSIZE long limb vector pointed to by MPTR
   to a printable string in STR in base BASE.

Copyright (C) 1991, 1992, 1993, 1994, 1996 Free Software Foundation, Inc.

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

/* Convert the limb vector pointed to by MPTR and MSIZE long to a
   char array, using base BASE for the result array.  Store the
   result in the character array STR.  STR must point to an array with
   space for the largest possible number represented by a MSIZE long
   limb vector + 1 extra character.

   The result is NOT in Ascii, to convert it to printable format, add
   '0' or 'A' depending on the base and range.

   Return the number of digits in the result string.
   This may include some leading zeros.

   The limb vector pointed to by MPTR is clobbered.  */

size_t
mpn_get_str (str, base, mptr, msize)
     unsigned char *str;
     int base;
     mp_ptr mptr;
     mp_size_t msize;
{
  mp_limb_t big_base;
#if UDIV_NEEDS_NORMALIZATION || UDIV_TIME > 2 * UMUL_TIME
  int normalization_steps;
#endif
#if UDIV_TIME > 2 * UMUL_TIME
  mp_limb_t big_base_inverted;
#endif
  unsigned int dig_per_u;
  mp_size_t out_len;
  register unsigned char *s;

  big_base = __mp_bases[base].big_base;

  s = str;

  /* Special case zero, as the code below doesn't handle it.  */
  if (msize == 0)
    {
      s[0] = 0;
      return 1;
    }

  if ((base & (base - 1)) == 0)
    {
      /* The base is a power of 2.  Make conversion from most
	 significant side.  */
      mp_limb_t n1, n0;
      register int bits_per_digit = big_base;
      register int x;
      register int bit_pos;
      register int i;

      n1 = mptr[msize - 1];
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
	      *s++ = (n1 >> bit_pos) & ((1 << bits_per_digit) - 1);
	      bit_pos -= bits_per_digit;
	    }
	  i--;
	  if (i < 0)
	    break;
	  n0 = (n1 << -bit_pos) & ((1 << bits_per_digit) - 1);
	  n1 = mptr[i];
	  bit_pos += BITS_PER_MP_LIMB;
	  *s++ = n0 | (n1 >> bit_pos);
	}

      *s = 0;

      return s - str;
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
      /* Get the fixed-point approximation to 1/(BIG_BASE << NORMALIZATION_STEPS).  */
      big_base_inverted = __mp_bases[base].big_base_inverted;
#endif
#endif

      dig_per_u = __mp_bases[base].chars_per_limb;
      out_len = ((size_t) msize * BITS_PER_MP_LIMB
		 * __mp_bases[base].chars_per_bit_exactly) + 1;
      s += out_len;

      while (msize != 0)
	{
	  int i;
	  mp_limb_t n0, n1;

#if UDIV_NEEDS_NORMALIZATION || UDIV_TIME > 2 * UMUL_TIME
	  /* If we shifted BIG_BASE above, shift the dividend too, to get
	     the right quotient.  We need to do this every loop,
	     since the intermediate quotients are OK, but the quotient from
	     one turn in the loop is going to be the dividend in the
	     next turn, and the dividend needs to be up-shifted.  */
	  if (normalization_steps != 0)
	    {
	      n0 = mpn_lshift (mptr, mptr, msize, normalization_steps);

	      /* If the shifting gave a carry out limb, store it and
		 increase the length.  */
	      if (n0 != 0)
		{
		  mptr[msize] = n0;
		  msize++;
		}
	    }
#endif

	  /* Divide the number at TP with BIG_BASE to get a quotient and a
	     remainder.  The remainder is our new digit in base BIG_BASE.  */
	  i = msize - 1;
	  n1 = mptr[i];

	  if (n1 >= big_base)
	    n1 = 0;
	  else
	    {
	      msize--;
	      i--;
	    }

	  for (; i >= 0; i--)
	    {
	      n0 = mptr[i];
#if UDIV_TIME > 2 * UMUL_TIME
	      udiv_qrnnd_preinv (mptr[i], n1, n1, n0, big_base, big_base_inverted);
#else
	      udiv_qrnnd (mptr[i], n1, n1, n0, big_base);
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
	      *--s = n1 % base;
	      n1 /= base;
	      if (n1 == 0 && msize == 0)
		break;
	    }
	}

      while (s != str)
	*--s = 0;
      return out_len;
    }
}
