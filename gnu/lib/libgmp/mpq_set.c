/* mpq_set(dest,src) -- Set DEST to SRC.

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

#include "gmp.h"
#include "gmp-impl.h"

void
#ifdef __STDC__
mpq_set (MP_RAT *dest, const MP_RAT *src)
#else
mpq_set (dest, src)
     MP_RAT *dest;
     const MP_RAT *src;
#endif
{
  mp_size num_size, den_size;
  mp_size abs_num_size;

  num_size = src->num.size;
  abs_num_size = ABS (num_size);
  if (dest->num.alloc < abs_num_size)
    _mpz_realloc (&(dest->num), abs_num_size);
  MPN_COPY (dest->num.d, src->num.d, abs_num_size);
  dest->num.size = num_size;

  den_size = src->den.size;
  if (dest->den.alloc < den_size)
    _mpz_realloc (&(dest->den), den_size);
  MPN_COPY (dest->den.d, src->den.d, den_size);
  dest->den.size = den_size;
}
