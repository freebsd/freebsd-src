/* mpq_div -- divide two rational numbers.

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
mpq_div (MP_RAT *quot, const MP_RAT *dividend, const MP_RAT *divisor)
#else
mpq_div (quot, dividend, divisor)
     MP_RAT *quot;
     const MP_RAT *dividend;
     const MP_RAT *divisor;
#endif
{
  MP_INT gcd1, gcd2;
  MP_INT tmp1, tmp2;
  MP_INT numtmp;

  mpz_init (&gcd1);
  mpz_init (&gcd2);
  mpz_init (&tmp1);
  mpz_init (&tmp2);
  mpz_init (&numtmp);

  /* QUOT might be identical to either operand, so don't store the
     result there until we are finished with the input operands.  We
     dare to overwrite the numerator of QUOT when we are finished
     with the numerators of DIVIDEND and DIVISOR.  */

  mpz_gcd (&gcd1, &(dividend->num), &(divisor->num));
  mpz_gcd (&gcd2, &(divisor->den), &(dividend->den));

  if (gcd1.size > 1 || gcd1.d[0] != 1)
    mpz_div (&tmp1, &(dividend->num), &gcd1);
  else
    mpz_set (&tmp1, &(dividend->num));

  if (gcd2.size > 1 || gcd2.d[0] != 1)
    mpz_div (&tmp2, &(divisor->den), &gcd2);
  else
    mpz_set (&tmp2, &(divisor->den));

  mpz_mul (&numtmp, &tmp1, &tmp2);

  if (gcd1.size > 1 || gcd1.d[0] != 1)
    mpz_div (&tmp1, &(divisor->num), &gcd1);
  else
    mpz_set (&tmp1, &(divisor->num));

  if (gcd2.size > 1 || gcd2.d[0] != 1)
    mpz_div (&tmp2, &(dividend->den), &gcd2);
  else
    mpz_set (&tmp2, &(dividend->den));

  mpz_mul (&(quot->den), &tmp1, &tmp2);

  /* We needed to go via NUMTMP to take care of QUOT being the same
     as either input operands.  Now move NUMTMP to QUOT->NUM.  */
  mpz_set (&(quot->num), &numtmp);

  /* Keep the denominator positive.  */
  if (quot->den.size < 0)
    {
      quot->den.size = -quot->den.size;
      quot->num.size = -quot->num.size;
    }

  mpz_clear (&numtmp);
  mpz_clear (&tmp2);
  mpz_clear (&tmp1);
  mpz_clear (&gcd2);
  mpz_clear (&gcd1);
}
