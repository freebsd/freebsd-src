/* xtom -- convert a hexadecimal string to a MINT, and return a pointer to
   the MINT.

Copyright (C) 1991, 1994, 1995 Free Software Foundation, Inc.

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

#include "mp.h"
#include "gmp.h"
#include "gmp-impl.h"

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

MINT *
#if __STDC__
xtom (const char *str)
#else
xtom (str)
     const char *str;
#endif
{
  size_t str_size;
  char *s, *begs;
  size_t i;
  mp_size_t xsize;
  int c;
  int negative;
  MINT *x = (MINT *) (*_mp_allocate_func) (sizeof (MINT));
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

  if (digit_value_in_base (c, 16) < 0)
    return 0;			/* error if no digits */

  TMP_MARK (marker);
  str_size = strlen (str - 1);
  s = begs = (char *) TMP_ALLOC (str_size + 1);

  for (i = 0; i < str_size; i++)
    {
      if (!isspace (c))
	{
	  int dig = digit_value_in_base (c, 16);
	  if (dig < 0)
	    {
	      TMP_FREE (marker);
	      return 0;
	    }
	  *s++ = dig;
	}
      c = *str++;
    }

  str_size = s - begs;

  xsize = str_size / __mp_bases[16].chars_per_limb + 1;
  x->_mp_alloc = xsize;
  x->_mp_d = (mp_ptr) (*_mp_allocate_func) (xsize * BYTES_PER_MP_LIMB);

  xsize = mpn_set_str (x->_mp_d, (unsigned char *) begs, str_size, 16);
  x->_mp_size = negative ? -xsize : xsize;

  TMP_FREE (marker);
  return x;
}
