/* mpz_abs(MP_INT *dst, MP_INT *src) -- Assign the absolute value of SRC to DST.

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
mpz_abs (MP_INT *dst, const MP_INT *src)
#else
mpz_abs (dst, src)
     MP_INT *dst;
     const MP_INT *src;
#endif
{
  mp_size src_size = ABS (src->size);

  if (src != dst)
    {
      if (dst->alloc < src_size)
	_mpz_realloc (dst, src_size);

      MPN_COPY (dst->d, src->d, src_size);
    }

  dst->size = src_size;
}
