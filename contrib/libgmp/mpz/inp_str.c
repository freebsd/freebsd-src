/* mpz_inp_str(dest_integer, stream, base) -- Input a number in base
   BASE from stdio stream STREAM and store the result in DEST_INTEGER.

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

#include <stdio.h>
#include <ctype.h>
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

size_t
#if __STDC__
mpz_inp_str (mpz_ptr dest, FILE *stream, int base)
#else
mpz_inp_str (dest, stream, base)
     mpz_ptr dest;
     FILE *stream;
     int base;
#endif
{
  char *str;
  size_t alloc_size, str_size;
  int c;
  int negative;
  mp_size_t dest_size;
  size_t nread;

  if (stream == 0)
    stream = stdin;

  alloc_size = 100;
  str = (char *) (*_mp_allocate_func) (alloc_size);
  str_size = 0;
  nread = 0;

  /* Skip whitespace.  */
  do
    {
      c = getc (stream);
      nread++;
    }
  while (isspace (c));

  negative = 0;
  if (c == '-')
    {
      negative = 1;
      c = getc (stream);
    }

  if (digit_value_in_base (c, base == 0 ? 10 : base) < 0)
    return 0;			/* error if no digits */

  /* If BASE is 0, try to find out the base by looking at the initial
     characters.  */
  if (base == 0)
    {
      base = 10;
      if (c == '0')
	{
	  base = 8;
	  c = getc (stream);
	  nread++;
	  if (c == 'x' || c == 'X')
	    {
	      base = 16;
	      c = getc (stream);
	      nread++;
	    }
	}
    }

  for (;;)
    {
      int dig;
      if (str_size >= alloc_size)
	{
	  size_t old_alloc_size = alloc_size;
	  alloc_size = alloc_size * 3 / 2;
	  str = (char *) (*_mp_reallocate_func) (str, old_alloc_size, alloc_size);
	}
      dig = digit_value_in_base (c, base);
      if (dig < 0)
	break;
      str[str_size++] = dig;
      c = getc (stream);
    }

  ungetc (c, stream);

  dest_size = str_size / __mp_bases[base].chars_per_limb + 1;
  if (dest->_mp_alloc < dest_size)
    _mpz_realloc (dest, dest_size);

  dest_size = mpn_set_str (dest->_mp_d, (unsigned char *) str, str_size, base);
  dest->_mp_size = negative ? -dest_size : dest_size;

  (*_mp_free_func) (str, alloc_size);
  return str_size + nread;
}
