/* sdiv -- Divide a MINT by a short integer.  Produce a MINT quotient
   and a short remainder.

Copyright (C) 1991, 1994, 1995 Free Software Foundation, Inc.

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

#include "mp.h"
#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

void
#if __STDC__
sdiv (const MINT *dividend, signed short int divisor_short, MINT *quot, short *rem_ptr)
#else
sdiv (dividend, divisor_short, quot, rem_ptr)
     const MINT *dividend;
     short int divisor_short;
     MINT *quot;
     short *rem_ptr;
#endif
{
  mp_size_t sign_dividend;
  signed long int sign_divisor;
  mp_size_t dividend_size, quot_size;
  mp_ptr dividend_ptr, quot_ptr;
  mp_limb_t divisor_limb;
  mp_limb_t remainder_limb;

  sign_dividend = dividend->_mp_size;
  dividend_size = ABS (dividend->_mp_size);

  if (dividend_size == 0)
    {
      quot->_mp_size = 0;
      *rem_ptr = 0;
      return;
    }

  sign_divisor = divisor_short;
  divisor_limb = ABS (divisor_short);

  /* No need for temporary allocation and copying even if QUOT == DIVIDEND
     as the divisor is just one limb, and thus no intermediate remainders
     need to be stored.  */

  if (quot->_mp_alloc < dividend_size)
    _mp_realloc (quot, dividend_size);

  quot_ptr = quot->_mp_d;
  dividend_ptr = dividend->_mp_d;

  remainder_limb = mpn_divmod_1 (quot_ptr,
				 dividend_ptr, dividend_size, divisor_limb);

  *rem_ptr = sign_dividend >= 0 ? remainder_limb : -remainder_limb;
  /* The quotient is DIVIDEND_SIZE limbs, but the most significant
     might be zero.  Set QUOT_SIZE properly. */
  quot_size = dividend_size - (quot_ptr[dividend_size - 1] == 0);
  quot->_mp_size = (sign_divisor ^ sign_dividend) >= 0 ? quot_size : -quot_size;
}
