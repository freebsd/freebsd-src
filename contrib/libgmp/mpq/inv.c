/* mpq_inv(dest,src) -- invert a rational number, i.e. set DEST to SRC
   with the numerator and denominator swapped.

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
mpq_inv (MP_RAT *dest, const MP_RAT *src)
#else
mpq_inv (dest, src)
     MP_RAT *dest;
     const MP_RAT *src;
#endif
{
  mp_size_t num_size = src->_mp_num._mp_size;
  mp_size_t den_size = src->_mp_den._mp_size;

  if (num_size == 0)
    num_size = 1 / num_size;	/* Divide by zero!  */

  if (num_size < 0)
    {
      num_size = -num_size;
      den_size = -den_size;
    }
  dest->_mp_den._mp_size = num_size;
  dest->_mp_num._mp_size = den_size;

  /* If dest == src we may just swap the numerator and denominator, but
     we have to ensure the new denominator is positive.  */

  if (dest == src)
    {
      mp_size_t alloc = dest->_mp_num._mp_alloc;
      mp_ptr limb_ptr = dest->_mp_num._mp_d;

      dest->_mp_num._mp_alloc = dest->_mp_den._mp_alloc;
      dest->_mp_num._mp_d = dest->_mp_den._mp_d;

      dest->_mp_den._mp_alloc = alloc;
      dest->_mp_den._mp_d = limb_ptr;
    }
  else
    {
      den_size = ABS (den_size);
      if (dest->_mp_num._mp_alloc < den_size)
	_mpz_realloc (&(dest->_mp_num), den_size);

      if (dest->_mp_den._mp_alloc < num_size)
	_mpz_realloc (&(dest->_mp_den), num_size);

      MPN_COPY (dest->_mp_num._mp_d, src->_mp_den._mp_d, den_size);
      MPN_COPY (dest->_mp_den._mp_d, src->_mp_num._mp_d, num_size);
    }
}
