/* mtox -- Convert OPERAND to hexadecimal and return a malloc'ed string
   with the result of the conversion.

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

#include "mp.h"
#include "gmp.h"
#include "gmp-impl.h"

char *
#if __STDC__
mtox (const MINT *x)
#else
mtox (x)
     const MINT *x;
#endif
{
  mp_ptr xp;
  mp_size_t xsize = x->_mp_size;
  mp_size_t xsign;
  unsigned char *str, *s;
  size_t str_size, i;
  int zeros;
  char *num_to_text;
  TMP_DECL (marker);

  if (xsize == 0)
    {
      str = (unsigned char *) (*_mp_allocate_func) (2);
      str[0] = '0';
      str[1] = 0;
      return str;
    }
  xsign = xsize;
  if (xsize < 0)
    xsize = -xsize;

  TMP_MARK (marker);
  str_size = ((size_t) (xsize * BITS_PER_MP_LIMB
			* __mp_bases[16].chars_per_bit_exactly)) + 3;
  str = (unsigned char *) (*_mp_allocate_func) (str_size);
  s = str;

  if (xsign < 0)
    *s++ = '-';

  /* Move the number to convert into temporary space, since mpn_get_str
     clobbers its argument + needs one extra high limb....  */
  xp = (mp_ptr) TMP_ALLOC ((xsize + 1) * BYTES_PER_MP_LIMB);
  MPN_COPY (xp, x->_mp_d, xsize);

  str_size = mpn_get_str (s, 16, xp, xsize);

  /* mpn_get_str might make some leading zeros.  Skip them.  */
  for (zeros = 0; s[zeros] == 0; zeros++)
    str_size--;

  /* Translate to printable chars and move string down.  */
  for (i = 0; i < str_size; i++)
    s[i] = "0123456789abcdef"[s[zeros + i]];
  s[str_size] = 0;

  return str;
}
