/* mpq_set(dest,src) -- Set DEST to SRC.

Copyright (C) 1991, 1994, 1995 Free Software Foundation, Inc.

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

#include "gmp.h"
#include "gmp-impl.h"

void
#if __STDC__
mpq_set (MP_RAT *dest, const MP_RAT *src)
#else
mpq_set (dest, src)
     MP_RAT *dest;
     const MP_RAT *src;
#endif
{
  mp_size_t num_size, den_size;
  mp_size_t abs_num_size;

  num_size = src->_mp_num._mp_size;
  abs_num_size = ABS (num_size);
  if (dest->_mp_num._mp_alloc < abs_num_size)
    _mpz_realloc (&(dest->_mp_num), abs_num_size);
  MPN_COPY (dest->_mp_num._mp_d, src->_mp_num._mp_d, abs_num_size);
  dest->_mp_num._mp_size = num_size;

  den_size = src->_mp_den._mp_size;
  if (dest->_mp_den._mp_alloc < den_size)
    _mpz_realloc (&(dest->_mp_den), den_size);
  MPN_COPY (dest->_mp_den._mp_d, src->_mp_den._mp_d, den_size);
  dest->_mp_den._mp_size = den_size;
}
