/* mpq_inv(dest,src) -- invert a rational number, i.e. set DEST to SRC
   with the numerator and denominator swapped.

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
mpq_inv (MP_RAT *dest, const MP_RAT *src)
#else
mpq_inv (dest, src)
     MP_RAT *dest;
     const MP_RAT *src;
#endif
{
  mp_size num_size = src->num.size;
  mp_size den_size = src->den.size;

  if (num_size == 0)
    num_size = 1 / num_size;	/* Divide by zero!  */

  if (num_size < 0)
    {
      num_size = -num_size;
      den_size = -den_size;
    }
  dest->den.size = num_size;
  dest->num.size = den_size;

  /* If dest == src we may just swap the numerator and denominator, but
     we have to ensure the new denominator is positive.  */

  if (dest == src)
    {
      mp_size alloc = dest->num.alloc;
      mp_ptr limb_ptr = dest->num.d;

      dest->num.alloc = dest->den.alloc;
      dest->num.d = dest->den.d;

      dest->den.alloc = alloc;
      dest->den.d = limb_ptr;
    }
  else
    {
      den_size = ABS (den_size);
      if (dest->num.alloc < den_size)
	_mpz_realloc (&(dest->num), den_size);

      if (dest->den.alloc < num_size)
	_mpz_realloc (&(dest->den), num_size);

      MPN_COPY (dest->num.d, src->den.d, den_size);
      MPN_COPY (dest->den.d, src->num.d, num_size);
    }
}
