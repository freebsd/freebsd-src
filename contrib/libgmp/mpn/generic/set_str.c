/* mpn_set_str (mp_ptr res_ptr, const char *str, size_t str_len, int base)
   -- Convert a STR_LEN long base BASE byte string pointed to by STR to a
   limb vector pointed to by RES_PTR.  Return the number of limbs in
   RES_PTR.

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

mp_size_t
mpn_set_str (xp, str, str_len, base)
     mp_ptr xp;
     const unsigned char *str;
     size_t str_len;
     int base;
{
  mp_size_t size;
  mp_limb_t big_base;
  int indigits_per_limb;
  mp_limb_t res_digit;

  big_base = __mp_bases[base].big_base;
  indigits_per_limb = __mp_bases[base].chars_per_limb;

/*   size = str_len / indigits_per_limb + 1;  */

  size = 0;

  if ((base & (base - 1)) == 0)
    {
      /* The base is a power of 2.  Read the input string from
	 least to most significant character/digit.  */

      const unsigned char *s;
      int next_bitpos;
      int bits_per_indigit = big_base;

      res_digit = 0;
      next_bitpos = 0;

      for (s = str + str_len - 1; s >= str; s--)
	{
	  int inp_digit = *s;

	  res_digit |= (mp_limb_t) inp_digit << next_bitpos;
	  next_bitpos += bits_per_indigit;
	  if (next_bitpos >= BITS_PER_MP_LIMB)
	    {
	      xp[size++] = res_digit;
	      next_bitpos -= BITS_PER_MP_LIMB;
	      res_digit = inp_digit >> (bits_per_indigit - next_bitpos);
	    }
	}

      if (res_digit != 0)
	xp[size++] = res_digit;
    }
  else
    {
      /* General case.  The base is not a power of 2.  */

      size_t i;
      int j;
      mp_limb_t cy_limb;

      for (i = indigits_per_limb; i < str_len; i += indigits_per_limb)
	{
	  res_digit = *str++;
	  if (base == 10)
	    { /* This is a common case.
		 Help the compiler to avoid multiplication.  */
	      for (j = 1; j < indigits_per_limb; j++)
		res_digit = res_digit * 10 + *str++;
	    }
	  else
	    {
	      for (j = 1; j < indigits_per_limb; j++)
		res_digit = res_digit * base + *str++;
	    }

	  if (size == 0)
	    {
	      if (res_digit != 0)
		{
		  xp[0] = res_digit;
		  size = 1;
		}
	    }
	  else
	    {
	      cy_limb = mpn_mul_1 (xp, xp, size, big_base);
	      cy_limb += mpn_add_1 (xp, xp, size, res_digit);
	      if (cy_limb != 0)
		xp[size++] = cy_limb;
	    }
	}

      big_base = base;
      res_digit = *str++;
      if (base == 10)
	{ /* This is a common case.
	     Help the compiler to avoid multiplication.  */
	  for (j = 1; j < str_len - (i - indigits_per_limb); j++)
	    {
	      res_digit = res_digit * 10 + *str++;
	      big_base *= 10;
	    }
	}
      else
	{
	  for (j = 1; j < str_len - (i - indigits_per_limb); j++)
	    {
	      res_digit = res_digit * base + *str++;
	      big_base *= base;
	    }
	}

      if (size == 0)
	{
	  if (res_digit != 0)
	    {
	      xp[0] = res_digit;
	      size = 1;
	    }
	}
      else
	{
	  cy_limb = mpn_mul_1 (xp, xp, size, big_base);
	  cy_limb += mpn_add_1 (xp, xp, size, res_digit);
	  if (cy_limb != 0)
	    xp[size++] = cy_limb;
	}
    }

  return size;
}
