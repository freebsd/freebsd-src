/* mpz_jacobi (op1, op2).
   Contributed by Bennet Yee (bsy) at Carnegie-Mellon University

Copyright (C) 1991, 1996 Free Software Foundation, Inc.

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

/* Precondition:  both p and q are positive */

int
#if	__STDC__
mpz_jacobi (mpz_srcptr pi, mpz_srcptr qi)
#else
mpz_jacobi (pi, qi)
     mpz_srcptr pi, qi;
#endif
{
#if GCDCHECK
  int retval;
  mpz_t gcdval;

  mpz_init (gcdval);
  mpz_gcd (gcdval, pi, qi);
  if (!mpz_cmp_ui (gcdval, 1L))
    {
      /* J(ab,cb) = J(ab,c)J(ab,b) = J(ab,c)J(0,b) = J(ab,c)*0 */
      retval = 0;
    }
  else
    retval = mpz_legendre (pi, qi);
  mpz_clear (gcdval);
  return retval;
#else
  return mpz_legendre (pi, qi);
#endif
}
