/* mpz_mod_ui(rem, dividend, divisor_limb)
   -- Set REM to DIVDEND mod DIVISOR_LIMB.

Copyright (C) 1991, 1992 Free Software Foundation, Inc.

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
#include "longlong.h"

void
#ifdef __STDC__
mpz_mod_ui (MP_INT *rem, const MP_INT *dividend,
	    unsigned long int divisor_limb)
#else
mpz_mod_ui (rem, dividend, divisor_limb)
     MP_INT *rem;
     const MP_INT *dividend;
     unsigned long int divisor_limb;
#endif
{
  mp_size sign_dividend;
  mp_size dividend_size;
  mp_limb remainder_limb;

  sign_dividend = dividend->size;
  dividend_size = ABS (dividend->size);

  if (dividend_size == 0)
    {
      rem->size = 0;
      return;
    }

  /* No need for temporary allocation and copying if QUOT == DIVIDEND as
     the divisor is just one limb, and thus no intermediate remainders
     need to be stored.  */

  remainder_limb = mpn_mod_1 (dividend->d, dividend_size, divisor_limb);

  if (remainder_limb == 0)
    rem->size = 0;
  else
    {
      /* Store the single-limb remainder.  We don't check if there's space
	 for just one limb, since no function ever makes zero space.  */
      rem->size = sign_dividend >= 0 ? 1 : -1;
      rem->d[0] = remainder_limb;
    }
}
