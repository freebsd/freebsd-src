/* mpz_fdiv_q -- Division rounding the quotient towards -infinity.
   The remainder gets the same sign as the denominator.

Copyright (C) 1994, 1995, 1996 Free Software Foundation, Inc.

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
mpz_fdiv_q (mpz_ptr quot, mpz_srcptr dividend, mpz_srcptr divisor)
#else
mpz_fdiv_q (quot, dividend, divisor)
     mpz_ptr quot;
     mpz_srcptr dividend;
     mpz_srcptr divisor;
#endif
{
  mp_size_t dividend_size = dividend->_mp_size;
  mp_size_t divisor_size = divisor->_mp_size;
  mpz_t rem;
  TMP_DECL (marker);

  TMP_MARK (marker);

  MPZ_TMP_INIT (rem, 1 + ABS (dividend_size));

  mpz_tdiv_qr (quot, rem, dividend, divisor);

  if ((divisor_size ^ dividend_size) < 0 && rem->_mp_size != 0)
    mpz_sub_ui (quot, quot, 1L);

  TMP_FREE (marker);
}
