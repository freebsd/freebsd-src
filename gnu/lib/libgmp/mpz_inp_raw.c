/* mpz_inp_raw -- Input a MP_INT in raw, but endianess, and wordsize
   independent format (as output by mpz_out_raw).

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
mpz_inp_raw (MP_INT *x, FILE *file)
#else
mpz_inp_raw (x, file)
     MP_INT *x;
     FILE *file;
#endif
{
  int i;
  mp_size s;
  mp_size xsize;
  mp_ptr xp;
  unsigned int c;
  mp_limb x_digit;
  mp_size x_index;

  xsize = 0;
  for (i = 4 - 1; i >= 0; i--)
    {
      c = fgetc (file);
      xsize = (xsize << BITS_PER_CHAR) | c;
    }

  /* ??? Sign extend xsize for non-32 bit machines?  */

  x_index = (ABS (xsize) + BYTES_PER_MP_LIMB - 1) / BYTES_PER_MP_LIMB - 1;

  if (x->alloc < x_index)
    _mpz_realloc (x, x_index);

  xp = x->d;
  x->size = xsize / BYTES_PER_MP_LIMB;
  x_digit = 0;
  for (s = ABS (xsize) - 1; s >= 0; s--)
    {
      i = s % BYTES_PER_MP_LIMB;
      c = fgetc (file);
      x_digit = (x_digit << BITS_PER_CHAR) | c;
      if (i == 0)
	{
	  xp[x_index--] = x_digit;
	  x_digit = 0;
	}
    }
}
