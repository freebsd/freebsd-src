/* mpq_mul -- mutiply two rational numbers.

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
mpq_mul (MP_RAT *prod, const MP_RAT *m1, const MP_RAT *m2)
#else
mpq_mul (prod, m1, m2)
     MP_RAT *prod;
     const MP_RAT *m1;
     const MP_RAT *m2;
#endif
{
  MP_INT gcd1, gcd2;
  MP_INT tmp1, tmp2;

  mpz_init (&gcd1);
  mpz_init (&gcd2);
  mpz_init (&tmp1);
  mpz_init (&tmp2);

  /* PROD might be identical to either operand, so don't store the
     result there until we are finished with the input operands.  We
     dare to overwrite the numerator of PROD when we are finished
     with the numerators of M1 and M1.  */

  mpz_gcd (&gcd1, &(m1->num), &(m2->den));
  mpz_gcd (&gcd2, &(m2->num), &(m1->den));

  if (gcd1.size > 1 || gcd1.d[0] != 1)
    mpz_div (&tmp1, &(m1->num), &gcd1);
  else
    mpz_set (&tmp1, &(m1->num));

  if (gcd2.size > 1 || gcd2.d[0] != 1)
    mpz_div (&tmp2, &(m2->num), &gcd2);
  else
    mpz_set (&tmp2, &(m2->num));

  mpz_mul (&(prod->num), &tmp1, &tmp2);

  if (gcd1.size > 1 || gcd1.d[0] != 1)
    mpz_div (&tmp1, &(m2->den), &gcd1);
  else
    mpz_set (&tmp1, &(m2->den));

  if (gcd2.size > 1 || gcd2.d[0] != 1)
    mpz_div (&tmp2, &(m1->den), &gcd2);
  else
    mpz_set (&tmp2, &(m1->den));

  mpz_mul (&(prod->den), &tmp1, &tmp2);

  mpz_clear (&tmp2);
  mpz_clear (&tmp1);
  mpz_clear (&gcd2);
  mpz_clear (&gcd1);
}
