/* mpz_out_raw -- Output a MP_INT in raw, but endianess-independent format.

Copyright (C) 1991, 1992 Free Software Foundation, Inc.

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

#include "gmp.h"
#include "gmp-impl.h"

void
#ifdef __STDC__
mpz_out_raw (FILE *file, const MP_INT *x)
#else
mpz_out_raw (file, x)
     FILE *file;
     const MP_INT *x;
#endif
{
  int i;
  mp_size s;
  mp_size xsize = x->size;
  mp_srcptr xp = x->d;
  mp_size out_size = xsize * BYTES_PER_MP_LIMB;

  /* Make the size 4 bytes on all machines, to make the format portable.  */
  for (i = 4 - 1; i >= 0; i--)
    fputc ((out_size >> (i * BITS_PER_CHAR)) % (1 << BITS_PER_CHAR), file);

  /* Output from the most significant digit to the least significant digit,
     with each digit also output in decreasing significance order.  */
  for (s = ABS (xsize) - 1; s >= 0; s--)
    {
      mp_limb x_digit;

      x_digit = xp[s];
      for (i = BYTES_PER_MP_LIMB - 1; i >= 0; i--)
	fputc ((x_digit >> (i * BITS_PER_CHAR)) % (1 << BITS_PER_CHAR), file);
    }
}
