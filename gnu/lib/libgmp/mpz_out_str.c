/* mpz_out_str(stream, base, integer) -- Output to STREAM the multi prec.
   integer INTEGER in base BASE.

Copyright (C) 1991 Free Software Foundation, Inc.

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
mpz_out_str (FILE *stream, int base, const MP_INT *x)
#else
mpz_out_str (stream, base, x)
     FILE *stream;
     int base;
     const MP_INT *x;
#endif
{
  char *str;
  size_t str_size;

  str_size = ((size_t) (ABS (x->size) * BITS_PER_MP_LIMB
			* __mp_bases[ABS (base)].chars_per_bit_exactly)) + 3;
  str = (char *) alloca (str_size);
  _mpz_get_str (str, base, x);
  fputs (str, stream);
  alloca (0);
}
