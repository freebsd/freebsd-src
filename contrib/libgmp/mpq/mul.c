/* mpq_mul -- mutiply two rational numbers.

Copyright (C) 1991, 1994, 1995, 1996 Free Software Foundation, Inc.

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
mpq_mul (mpq_ptr prod, mpq_srcptr op1, mpq_srcptr op2)
#else
mpq_mul (prod, op1, op2)
     mpq_ptr prod;
     mpq_srcptr op1;
     mpq_srcptr op2;
#endif
{
  mpz_t gcd1, gcd2;
  mpz_t tmp1, tmp2;

  mpz_init (gcd1);
  mpz_init (gcd2);
  mpz_init (tmp1);
  mpz_init (tmp2);

  /* PROD might be identical to either operand, so don't store the
     result there until we are finished with the input operands.  We
     dare to overwrite the numerator of PROD when we are finished
     with the numerators of OP1 and OP2.  */

  mpz_gcd (gcd1, &(op1->_mp_num), &(op2->_mp_den));
  mpz_gcd (gcd2, &(op2->_mp_num), &(op1->_mp_den));

  if (gcd1->_mp_size > 1 || gcd1->_mp_d[0] != 1)
    mpz_divexact (tmp1, &(op1->_mp_num), gcd1);
  else
    mpz_set (tmp1, &(op1->_mp_num));

  if (gcd2->_mp_size > 1 || gcd2->_mp_d[0] != 1)
    mpz_divexact (tmp2, &(op2->_mp_num), gcd2);
  else
    mpz_set (tmp2, &(op2->_mp_num));

  mpz_mul (&(prod->_mp_num), tmp1, tmp2);

  if (gcd1->_mp_size > 1 || gcd1->_mp_d[0] != 1)
    mpz_divexact (tmp1, &(op2->_mp_den), gcd1);
  else
    mpz_set (tmp1, &(op2->_mp_den));

  if (gcd2->_mp_size > 1 || gcd2->_mp_d[0] != 1)
    mpz_divexact (tmp2, &(op1->_mp_den), gcd2);
  else
    mpz_set (tmp2, &(op1->_mp_den));

  mpz_mul (&(prod->_mp_den), tmp1, tmp2);

  mpz_clear (tmp2);
  mpz_clear (tmp1);
  mpz_clear (gcd2);
  mpz_clear (gcd1);
}
