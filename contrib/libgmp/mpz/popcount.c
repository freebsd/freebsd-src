/* mpz_popcount(mpz_ptr op) -- Population count of OP.  If the operand is
   negative, return ~0 (a novel representation of infinity).

Copyright (C) 1994, 1996 Free Software Foundation, Inc.

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

unsigned long int
#if __STDC__
mpz_popcount (mpz_srcptr u)
#else
mpz_popcount (u)
     mpz_srcptr u;
#endif
{
  mp_size_t usize;

  usize = u->_mp_size;

  if ((usize) < 0)
    return ~ (unsigned long int) 0;

  return mpn_popcount (u->_mp_d, usize);
}
