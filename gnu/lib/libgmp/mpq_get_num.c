 /* mpq_get_num(num,rat_src) -- Set NUM to the numerator of RAT_SRC.

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
mpq_get_num (MP_INT *num, const MP_RAT *src)
#else
mpq_get_num (num, src)
     MP_INT *num;
     const MP_RAT *src;
#endif
{
  mp_size size = src->num.size;
  mp_size abs_size = ABS (size);

  if (num->alloc < abs_size)
    _mpz_realloc (num, abs_size);

  MPN_COPY (num->d, src->num.d, abs_size);
  num->size = size;
}
