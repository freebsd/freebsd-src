/* mpq_sub -- subtract two rational numbers.

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
mpq_sub (MP_RAT *dif, const MP_RAT *min, const MP_RAT *sub)
#else
mpq_sub (dif, min, sub)
     MP_RAT *dif;
     const MP_RAT *min;
     const MP_RAT *sub;
#endif
{
  MP_INT gcd1, gcd2;
  MP_INT tmp1, tmp2;

  mpz_init (&gcd1);
  mpz_init (&gcd2);
  mpz_init (&tmp1);
  mpz_init (&tmp2);

  /* DIF might be identical to either operand, so don't store the
     result there until we are finished with the input operands.  We
     dare to overwrite the numerator of DIF when we are finished
     with the numerators of MIN and SUB.  */

  mpz_gcd (&gcd1, &(min->den), &(sub->den));
  if (gcd1.size > 1 || gcd1.d[0] != 1)
    {
      MP_INT t;

      mpz_init (&t);

      mpz_div (&tmp1, &(sub->den), &gcd1);
      mpz_mul (&tmp1, &(min->num), &tmp1);

      mpz_div (&tmp2, &(min->den), &gcd1);
      mpz_mul (&tmp2, &(sub->num), &tmp2);

      mpz_sub (&t, &tmp1, &tmp2);
      mpz_gcd (&gcd2, &t, &gcd1);

      mpz_div (&(dif->num), &t, &gcd2);

      mpz_div (&tmp1, &(min->den), &gcd1);
      mpz_div (&tmp2, &(sub->den), &gcd2);
      mpz_mul (&(dif->den), &tmp1, &tmp2);

      mpz_clear (&t);
    }
  else
    {
      /* The common divisior is 1.  This is the case (for random input) with
	 probability 6/(pi**2).  */
      mpz_mul (&tmp1, &(min->num), &(sub->den));
      mpz_mul (&tmp2, &(sub->num), &(min->den));
      mpz_sub (&(dif->num), &tmp1, &tmp2);
      mpz_mul (&(dif->den), &(min->den), &(sub->den));
    }

  mpz_clear (&tmp2);
  mpz_clear (&tmp1);
  mpz_clear (&gcd2);
  mpz_clear (&gcd1);
}
