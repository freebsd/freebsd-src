/* mpz_out_raw -- Output a mpz_t in binary.  Use an endianess and word size
   independent format.

Copyright (C) 1995 Free Software Foundation, Inc.

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

#include "gmp.h"
#include "gmp-impl.h"

size_t
#if __STDC__
mpz_out_raw (FILE *stream, mpz_srcptr x)
#else
mpz_out_raw (stream, x)
     FILE *stream;
     mpz_srcptr x;
#endif
{
  int i;
  mp_size_t s;
  mp_size_t xsize = ABS (x->_mp_size);
  mp_srcptr xp = x->_mp_d;
  mp_size_t out_bytesize;
  mp_limb_t hi_limb;
  int n_bytes_in_hi_limb;

  if (stream == 0)
    stream = stdout;

  if (xsize == 0)
    {
      for (i = 4 - 1; i >= 0; i--)
	fputc (0, stream);
      return ferror (stream) ? 0 : 4;
    }

  hi_limb = xp[xsize - 1];
  for (i = BYTES_PER_MP_LIMB - 1; i > 0; i--)
    {
      if ((hi_limb >> i * BITS_PER_CHAR) != 0)
	break;
    }
  n_bytes_in_hi_limb = i + 1;
  out_bytesize = BYTES_PER_MP_LIMB * (xsize - 1) + n_bytes_in_hi_limb;
  if (x->_mp_size < 0)
    out_bytesize = -out_bytesize;

  /* Make the size 4 bytes on all machines, to make the format portable.  */
  for (i = 4 - 1; i >= 0; i--)
    fputc ((out_bytesize >> (i * BITS_PER_CHAR)) % (1 << BITS_PER_CHAR),
	   stream);

  /* Output from the most significant limb to the least significant limb,
     with each limb also output in decreasing significance order.  */

  /* Output the most significant limb separately, since we will only
     output some of its bytes.  */
  for (i = n_bytes_in_hi_limb - 1; i >= 0; i--)
    fputc ((hi_limb >> (i * BITS_PER_CHAR)) % (1 << BITS_PER_CHAR), stream);

  /* Output the remaining limbs.  */
  for (s = xsize - 2; s >= 0; s--)
    {
      mp_limb_t x_limb;

      x_limb = xp[s];
      for (i = BYTES_PER_MP_LIMB - 1; i >= 0; i--)
	fputc ((x_limb >> (i * BITS_PER_CHAR)) % (1 << BITS_PER_CHAR), stream);
    }
  return ferror (stream) ? 0 : ABS (out_bytesize) + 4;
}
