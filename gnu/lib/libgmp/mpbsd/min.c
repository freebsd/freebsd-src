/* min(MINT) -- Do decimal input from standard input and store result in
   MINT.

Copyright (C) 1991, 1994 Free Software Foundation, Inc.

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
#include "mp.h"
#include "gmp.h"
#include "gmp-impl.h"

void
#if __STDC__
min (MINT *dest)
#else
min (dest)
     MINT *dest;
#endif
{
  char *str;
  size_t alloc_size, str_size;
  int c;
  int negative;
  mp_size_t dest_size;

  alloc_size = 100;
  str = (char *) (*_mp_allocate_func) (alloc_size);
  str_size = 0;

  /* Skip whitespace.  */
  do
    c = getc (stdin);
  while (isspace (c));

  negative = 0;
  if (c == '-')
    {
      negative = 1;
      c = getc (stdin);
    }

  if (digit_value_in_base (c, 10) < 0)
    return;			/* error if no digits */

  for (;;)
    {
      int dig;
      if (str_size >= alloc_size)
	{
	  size_t old_alloc_size = alloc_size;
	  alloc_size = alloc_size * 3 / 2;
	  str = (char *) (*_mp_reallocate_func) (str, old_alloc_size, alloc_size);
	}
      dig = digit_value_in_base (c, 10);
      if (dig < 0)
	break;
      str[str_size++] = dig;
      c = getc (stdin);
    }

  ungetc (c, stdin);

  dest_size = str_size / __mp_bases[10].chars_per_limb + 1;
  if (dest->_mp_alloc < dest_size)
    _mp_realloc (dest, dest_size);

  dest_size = mpn_set_str (dest->_mp_d, (unsigned char *) str, str_size, 10);
  dest->_mp_size = negative ? -dest_size : dest_size;

  (*_mp_free_func) (str, alloc_size);
  return;
}
