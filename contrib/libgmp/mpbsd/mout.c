/* mout(MINT) -- Do decimal output of MINT to standard output.

Copyright (C) 1991, 1994, 1996 Free Software Foundation, Inc.

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
#include "mp.h"
#include "gmp.h"
#include "gmp-impl.h"

void
#if __STDC__
mout (const MINT *x)
#else
mout (x)
     const MINT *x;
#endif
{
  mp_ptr xp;
  mp_size_t x_size = x->_mp_size;
  unsigned char *str;
  size_t str_size;
  char *num_to_text;
  int i;
  TMP_DECL (marker);

  if (x_size == 0)
    {
      fputc ('0', stdout);
      return;
    }
  if (x_size < 0)
    {
      fputc ('-', stdout);
      x_size = -x_size;
    }

  TMP_MARK (marker);
  str_size = ((size_t) (x_size * BITS_PER_MP_LIMB
			* __mp_bases[10].chars_per_bit_exactly)) + 3;
  str = (unsigned char *) TMP_ALLOC (str_size);

  /* Move the number to convert into temporary space, since mpn_get_str
     clobbers its argument + needs one extra high limb....  */
  xp = (mp_ptr) TMP_ALLOC ((x_size + 1) * BYTES_PER_MP_LIMB);
  MPN_COPY (xp, x->_mp_d, x_size);

  str_size = mpn_get_str (str, 10, xp, x_size);

  /* mpn_get_str might make some leading zeros.  Skip them.  */
  while (*str == 0)
    {
      str_size--;
      str++;
    }

  /* Translate to printable chars.  */
  for (i = 0; i < str_size; i++)
    str[i] = "0123456789"[str[i]];
  str[str_size] = 0;

  str_size = strlen (str);
  if (str_size % 10 != 0)
    {
      fwrite (str, 1, str_size % 10, stdout);
      str += str_size % 10;
      str_size -= str_size % 10;
      if (str_size != 0)
	fputc (' ', stdout);
    }
  for (i = 0; i < str_size; i += 10)
    {
      fwrite (str, 1, 10, stdout);
      str += 10;
      if (i + 10 < str_size)
	fputc (' ', stdout);
    }
  fputc ('\n', stdout);
  TMP_FREE (marker);
}
