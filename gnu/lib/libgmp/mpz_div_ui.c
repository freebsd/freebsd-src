/* mpz_div_ui(quot, dividend, divisor_limb)
   -- Divide DIVIDEND by DIVISOR_LIMB and store the result in QUOT.

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
mpz_div_ui (MP_INT *quot, const MP_INT *dividend, unsigned long int divisor_limb)
#else
mpz_div_ui (quot, dividend, divisor_limb)
     MP_INT *quot;
     const MP_INT *dividend;
     unsigned long int divisor_limb;
#endif
{
  mp_size sign_dividend;
  mp_size dividend_size, quot_size;
  mp_ptr dividend_ptr, quot_ptr;

  sign_dividend = dividend->size;
  dividend_size = ABS (dividend->size);

  if (dividend_size == 0)
    {
      quot->size = 0;
      return;
    }

  /* No need for temporary allocation and copying if QUOT == DIVIDEND as
     the divisor is just one limb, and thus no intermediate remainders
     need to be stored.  */

  if (quot->alloc < dividend_size)
    _mpz_realloc (quot, dividend_size);

  quot_ptr = quot->d;
  dividend_ptr = dividend->d;

  mpn_divmod_1 (quot_ptr, dividend_ptr, dividend_size, divisor_limb);

  /* The quotient is DIVIDEND_SIZE limbs, but the most significant
     might be zero.  Set QUOT_SIZE properly. */
  quot_size = dividend_size - (quot_ptr[dividend_size - 1] == 0);
  quot->size = sign_dividend >= 0 ? quot_size : -quot_size;
}
