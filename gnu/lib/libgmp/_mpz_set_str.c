/* _mpz_set_str(mp_dest, string, base) -- Convert the \0-terminated
   string STRING in base BASE to multiple precision integer in
   MP_DEST.  Allow white space in the string.  If BASE == 0 determine
   the base in the C standard way, i.e.  0xhh...h means base 16,
   0oo...o means base 8, otherwise assume base 10.

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

#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

enum char_type
{
  XX = -3,
  SPC = -2,
  EOF = -1
};

static signed char ascii_to_num[256] =
{
  EOF,XX, XX, XX, XX, XX, XX, XX, XX, SPC,SPC,XX, XX, XX, XX, XX,
  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
  SPC,XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  XX, XX, XX, XX, XX, XX,
  XX, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
  25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, XX, XX, XX, XX, XX,
  XX, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
  25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, XX, XX, XX, XX, XX,
  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX,
  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX
};

int
#ifdef __STDC__
_mpz_set_str (MP_INT *x, const char *str, int base)
#else
_mpz_set_str (x, str, base)
     MP_INT *x;
     const char *str;
     int base;
#endif
{
  mp_ptr xp;
  mp_size size;
  mp_limb big_base;
  int indigits_per_limb;
  int negative = 0;
  int inp_rawchar;
  mp_limb inp_digit;
  mp_limb res_digit;
  size_t str_len;
  mp_size i;

  if (str[0] == '-')
    {
      negative = 1;
      str++;
    }

  if (base == 0)
    {
      if (str[0] == '0')
	{
	  if (str[1] == 'x' || str[1] == 'X')
	    base = 16;
	  else
	    base = 8;
	}
      else
	base = 10;
    }

  big_base = __mp_bases[base].big_base;
  indigits_per_limb = __mp_bases[base].chars_per_limb;

  str_len = strlen (str);

  size = str_len / indigits_per_limb + 1;
  if (x->alloc < size)
    _mpz_realloc (x, size);
  xp = x->d;

  size = 0;

  if ((base & (base - 1)) == 0)
    {
      /* The base is a power of 2.  Read the input string from
	 least to most significant character/digit.  */

      const char *s;
      int next_bitpos;
      int bits_per_indigit = big_base;

      /* Accept and ignore 0x or 0X before hexadecimal numbers.  */
      if (base == 16 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
	  str += 2;
	  str_len -= 2;
	}

      res_digit = 0;
      next_bitpos = 0;

      for (s = str + str_len - 1; s >= str; s--)
	{
	  inp_rawchar = *s;
	  inp_digit = ascii_to_num[inp_rawchar];

	  if (inp_digit >= base)
	    {
	      /* Was it white space?  Just ignore it.  */
	      if ((char) inp_digit == (char) SPC)
		continue;

	      /* We found rubbish in the string.  Return -1 to indicate
		 the error.  */
	      return -1;
	    }

	  res_digit |= inp_digit << next_bitpos;
	  next_bitpos += bits_per_indigit;
	  if (next_bitpos >= BITS_PER_MP_LIMB)
	    {
	      xp[size] = res_digit;
	      size++;
	      next_bitpos -= BITS_PER_MP_LIMB;
	      res_digit = inp_digit >> (bits_per_indigit - next_bitpos);
	    }
	}

      xp[size] = res_digit;
      size++;
      for (i = size - 1; i >= 0; i--)
	{
	  if (xp[i] != 0)
	    break;
	}
      size = i + 1;
    }
  else
    {
      /* General case.  The base is not a power of 2.  */

      mp_size i;
      int j;
      mp_limb cy;

      for (;;)
	{
	  res_digit = 0;
	  for (j = 0; j < indigits_per_limb; )
	    {
	      inp_rawchar = (unsigned char) *str++;
	      inp_digit = ascii_to_num[inp_rawchar];

	      /* Negative means that the character was not a proper digit.  */
	      if (inp_digit >= base)
		{
		  /* Was it white space?  Just ignore it.  */
		  if ((char) inp_digit == (char) SPC)
		    continue;

		  goto end_or_error;
		}

	      res_digit = res_digit * base + inp_digit;

	      /* Increment the loop counter here, since it mustn't be
		 incremented when we do "continue" above.  */
	      j++;
	    }

	  cy = res_digit;

	  /* Insert RES_DIGIT into the result multi prec integer.  */
	  for (i = 0; i < size; i++)
	    {
	      mp_limb p1, p0;
	      umul_ppmm (p1, p0, big_base, xp[i]);
	      p0 += cy;
	      cy = p1 + (p0 < cy);
	      xp[i] = p0;
	    }
	  if (cy != 0)
	    {
	      xp[size] = cy;
	      size++;
	    }
	}

    end_or_error:
      /* We probably have some digits in RES_DIGIT  (J tells how many).  */
      if ((char) inp_digit != (char) EOF)
	{
	  /* Error return.  */
	  return -1;
	}

      /* J contains number of digits (in base BASE) remaining in
	 RES_DIGIT.  */
      if (j > 0)
	{
	  big_base = 1;
	  do
	    {
	      big_base *= base;
	      j--;
	    }
	  while (j > 0);

	  cy = res_digit;

	  /* Insert ultimate RES_DIGIT into the result multi prec integer.  */
	  for (i = 0; i < size; i++)
	    {
	      mp_limb p1, p0;
	      umul_ppmm (p1, p0, big_base, xp[i]);
	      p0 += cy;
	      cy = p1 + (p0 < cy);
	      xp[i] = p0;
	    }
	  if (cy != 0)
	    {
	      xp[size] = cy;
	      size++;
	    }
	}
    }

  if (negative)
    size = -size;
  x->size = size;

  return 0;
}
