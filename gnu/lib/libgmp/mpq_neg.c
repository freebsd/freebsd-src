/* mpq_neg(dst, src) -- Assign the negated value of SRC to DST.

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
mpq_neg (MP_RAT *dst, const MP_RAT *src)
#else
mpq_neg (dst, src)
     MP_RAT *dst;
     const MP_RAT *src;
#endif
{
  mpz_neg (&dst->num, &src->num);
  mpz_set (&dst->den, &src->den);
}
