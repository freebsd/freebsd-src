/* mpz_inp_str(dest_integer, stream, base) -- Input a number in base
   BASE from stdio stream STREAM and store the result in DEST_INTEGER.

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

#include <stdio.h>
#include <ctype.h>
#include "gmp.h"
#include "gmp-impl.h"

static int
char_ok_for_base (c, base)
     int c;
     int base;
{
  if (isdigit (c))
    return (unsigned) c - '0' < base;
  if (islower (c))
    return (unsigned) c - 'a' + 10 < base;
  if (isupper (c))
    return (unsigned) c - 'A' + 10 < base;

  return 0;
}

void
#ifdef __STDC__
mpz_inp_str (MP_INT *dest, FILE *stream, int base)
#else
mpz_inp_str (dest, stream, base)
     MP_INT *dest;
     FILE *stream;
     int base;
#endif
{
  char *str;
  size_t str_size;
  size_t i;
  int c;
  int negative = 0;

  str_size = 100;
  str = (char *) (*_mp_allocate_func) (str_size);

  c = getc (stream);
  if (c == '-')
    {
      negative = 1;
      c = getc (stream);
    }

  /* If BASE is 0, try to find out the base by looking at the initial
     characters.  */
  if (base == 0)
    {
      base = 10;
      if (c == '0')
	{
	  base = 8;
	  c = getc (stream);
	  if (c == 'x' || c == 'X')
	    {
	      base = 16;
	      c = getc (stream);
	    }
	}
    }

  for (i = 0; char_ok_for_base (c, base); i++)
    {
      if (i >= str_size)
	{
	  size_t old_str_size = str_size;
	  str_size = str_size * 3 / 2;
	  str = (char *) (*_mp_reallocate_func) (str, old_str_size, str_size);
	}
      str[i] = c;
      c = getc (stream);
    }

  ungetc (c, stream);

  str[i] = 0;
  _mpz_set_str (dest, str, base);
  if (negative)
    dest->size = -dest->size;

  (*_mp_free_func) (str, str_size);
}
