/* mpz_tdiv_qr_ui(quot,rem,dividend,short_divisor) --
   Set QUOT to DIVIDEND / SHORT_DIVISOR
   and REM to DIVIDEND mod SHORT_DIVISOR.

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

void
#if __STDC__
mpz_tdiv_qr_ui (mpz_ptr quot, mpz_ptr rem, mpz_srcptr dividend, unsigned long int divisor)
#else
mpz_tdiv_qr_ui (quot, rem, dividend, divisor)
     mpz_ptr quot;
     mpz_ptr rem;
     mpz_srcptr dividend;
     unsigned long int divisor;
#endif
{
  mp_size_t dividend_size;
  mp_size_t size;
  mp_ptr quot_ptr;
  mp_limb_t remainder_limb;

  dividend_size = dividend->_mp_size;
  size = ABS (dividend_size);

  if (size == 0)
    {
      quot->_mp_size = 0;
      rem->_mp_size = 0;
      return;
    }

  /* No need for temporary allocation and copying if QUOT == DIVIDEND as
     the divisor is just one limb, and thus no intermediate remainders
     need to be stored.  */

  if (quot->_mp_alloc < size)
    _mpz_realloc (quot, size);

  quot_ptr = quot->_mp_d;

  remainder_limb = mpn_divmod_1 (quot_ptr, dividend->_mp_d, size,
				   (mp_limb_t) divisor);

  if (remainder_limb == 0)
    rem->_mp_size = 0;
  else
    {
      /* Store the single-limb remainder.  We don't check if there's space
	 for just one limb, since no function ever makes zero space.  */
      rem->_mp_size = dividend_size >= 0 ? 1 : -1;
      rem->_mp_d[0] = remainder_limb;
    }

  /* The quotient is SIZE limbs, but the most significant might be zero. */
  size -= quot_ptr[size - 1] == 0;
  quot->_mp_size = dividend_size >= 0 ? size : -size;
}
