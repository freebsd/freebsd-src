/* mpz_perfect_square_p(arg) -- Return non-zero if ARG is a perfect square,
   zero otherwise.

Copyright (C) 1991, 1993, 1994, 1996 Free Software Foundation, Inc.

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

int
#if __STDC__
mpz_perfect_square_p (mpz_srcptr a)
#else
mpz_perfect_square_p (a)
     mpz_srcptr a;
#endif
{
  mp_size_t asize = a->_mp_size;

  /* No negative numbers are perfect squares.  */
  if (asize < 0)
    return 0;

  return mpn_perfect_square_p (a->_mp_d, asize);
}
