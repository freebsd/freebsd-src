/* mpz_inp_raw -- Input a mpz_t in raw, but endianess, and wordsize
   independent format (as output by mpz_out_raw).

Copyright (C) 1991, 1993, 1994, 1995 Free Software Foundation, Inc.

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
mpz_inp_raw (mpz_ptr x, FILE *stream)
#else
mpz_inp_raw (x, stream)
     mpz_ptr x;
     FILE *stream;
#endif
{
  int i;
  mp_size_t s;
  mp_size_t xsize;
  mp_ptr xp;
  unsigned int c;
  mp_limb_t x_limb;
  mp_size_t in_bytesize;
  int neg_flag;

  if (stream == 0)
    stream = stdin;

  /* Read 4-byte size */
  in_bytesize = 0;
  for (i = 4 - 1; i >= 0; i--)
    {
      c = fgetc (stream);
      in_bytesize = (in_bytesize << BITS_PER_CHAR) | c;
    }

  /* Size is stored as a 32 bit word; sign extend in_bytesize for non-32 bit
     machines.  */
  if (sizeof (mp_size_t) > 4)
    in_bytesize |= (-(in_bytesize < 0)) << 31;

  neg_flag = in_bytesize < 0;
  in_bytesize = ABS (in_bytesize);
  xsize = (in_bytesize + BYTES_PER_MP_LIMB - 1) / BYTES_PER_MP_LIMB;

  if (xsize == 0)
    {
      x->_mp_size = 0;
      return 4;			/* we've read 4 bytes */
    }

  if (x->_mp_alloc < xsize)
    _mpz_realloc (x, xsize);
  xp = x->_mp_d;

  x_limb = 0;
  for (i = (in_bytesize - 1) % BYTES_PER_MP_LIMB; i >= 0; i--)
    {
      c = fgetc (stream);
      x_limb = (x_limb << BITS_PER_CHAR) | c;
    }
  xp[xsize - 1] = x_limb;

  for (s = xsize - 2; s >= 0; s--)
    {
      x_limb = 0;
      for (i = BYTES_PER_MP_LIMB - 1; i >= 0; i--)
	{
	  c = fgetc (stream);
	  x_limb = (x_limb << BITS_PER_CHAR) | c;
	}
      xp[s] = x_limb;
    }

  if (c == EOF)
    return 0;			/* error */

  MPN_NORMALIZE (xp, xsize);
  x->_mp_size = neg_flag ? -xsize : xsize;
  return in_bytesize + 4;
}
