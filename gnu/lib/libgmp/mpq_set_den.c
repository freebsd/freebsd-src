/* mpq_set_den(dest,den) -- Set the denominator of DEST from DEN.
   If DEN < 0 change the sign of the numerator of DEST.

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
mpq_set_den (MP_RAT *dest, const MP_INT *den)
#else
mpq_set_den (dest, den)
     MP_RAT *dest;
     const MP_INT *den;
#endif
{
  mp_size size = den->size;
  mp_size abs_size = ABS (size);

  if (dest->den.alloc < abs_size)
    _mpz_realloc (&(dest->den), abs_size);

  MPN_COPY (dest->den.d, den->d, abs_size);
  dest->den.size = abs_size;

  /* The denominator is always positive; move the sign to the numerator.  */
  if (size < 0)
    dest->num.size = -dest->num.size;
}
