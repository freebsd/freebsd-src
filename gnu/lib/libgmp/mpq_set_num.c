/* mpq_set_num(dest,num) -- Set the numerator of DEST from NUM.

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
mpq_set_num (MP_RAT *dest, const MP_INT *num)
#else
mpq_set_num (dest, num)
     MP_RAT *dest;
     const MP_INT *num;
#endif
{
  mp_size size = num->size;
  mp_size abs_size = ABS (size);

  if (dest->num.alloc < abs_size)
    _mpz_realloc (&(dest->num), abs_size);

  MPN_COPY (dest->num.d, num->d, abs_size);
  dest->num.size = size;
}
