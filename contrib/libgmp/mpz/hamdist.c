/* mpz_hamdist(mpz_ptr op1, mpz_ptr op2) -- Compute the hamming distance
   between OP1 and OP2.  If one of the operands is negative, return ~0.  (We
   could make the function well-defined when both operands are negative, but
   that would probably not be worth the trouble.

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
mpz_hamdist (mpz_srcptr u, mpz_srcptr v)
#else
mpz_hamdist (u, v)
     mpz_srcptr u;
     mpz_srcptr v;
#endif
{
  mp_srcptr up, vp;
  mp_size_t usize, vsize, size;
  unsigned long int count;

  usize = u->_mp_size;
  vsize = v->_mp_size;

  if ((usize | vsize) < 0)
    return ~ (unsigned long int) 0;

  up = u->_mp_d;
  vp = v->_mp_d;

  if (usize > vsize)
    {
      count = mpn_popcount (up + vsize, usize - vsize);
      size = vsize;
    }
  else
    {
      count = mpn_popcount (vp + usize, vsize - usize);
      size = usize;
    }

  return count + mpn_hamdist (up, vp, size);
}
