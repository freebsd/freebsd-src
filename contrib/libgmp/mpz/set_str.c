/* mpz_set_str(mp_dest, string, base) -- Convert the \0-terminated
   string STRING in base BASE to multiple precision integer in
   MP_DEST.  Allow white space in the string.  If BASE == 0 determine
   the base in the C standard way, i.e.  0xhh...h means base 16,
   0oo...o means base 8, otherwise assume base 10.

Copyright (C) 1991, 1993, 1994, Free Software Foundation, Inc.

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

#include <ctype.h>
#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

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
mpz_set_str (mpz_ptr x, const char *str, int base)
#else
mpz_set_str (x, str, base)
     mpz_ptr x;
     const char *str;
     int base;
#endif
{
  size_t str_size;
  char *s, *begs;
  size_t i;
  mp_size_t xsize;
  int c;
  int negative;
  TMP_DECL (marker);

  /* Skip whitespace.  */
  do
    c = *str++;
  while (isspace (c));

  negative = 0;
  if (c == '-')
    {
      negative = 1;
      c = *str++;
    }

  if (digit_value_in_base (c, base == 0 ? 10 : base) < 0)
    return -1;			/* error if no digits */

  /* If BASE is 0, try to find out the base by looking at the initial
     characters.  */
  if (base == 0)
    {
      base = 10;
      if (c == '0')
	{
	  base = 8;
	  c = *str++;
	  if (c == 'x' || c == 'X')
	    {
	      base = 16;
	      c = *str++;
	    }
	}
    }

  TMP_MARK (marker);
  str_size = strlen (str - 1);
  s = begs = (char *) TMP_ALLOC (str_size + 1);

  for (i = 0; i < str_size; i++)
    {
      if (!isspace (c))
	{
	  int dig = digit_value_in_base (c, base);
	  if (dig < 0)
	    {
	      TMP_FREE (marker);
	      return -1;
	    }
	  *s++ = dig;
	}
      c = *str++;
    }

  str_size = s - begs;

  xsize = str_size / __mp_bases[base].chars_per_limb + 1;
  if (x->_mp_alloc < xsize)
    _mpz_realloc (x, xsize);

  xsize = mpn_set_str (x->_mp_d, (unsigned char *) begs, str_size, base);
  x->_mp_size = negative ? -xsize : xsize;

  TMP_FREE (marker);
  return 0;
}
