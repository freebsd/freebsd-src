/* mpz_legendre (op1, op2).
   Contributed by Bennet Yee (bsy) at Carnegie-Mellon University

Copyright (C) 1992, 1996 Free Software Foundation, Inc.

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

#if defined (DEBUG)
#include <stdio.h>
#endif

/* Precondition:  both p and q are positive */

int
#if __STDC__
mpz_legendre (mpz_srcptr pi, mpz_srcptr qi)
#else
mpz_legendre (pi, qi)
mpz_srcptr pi, qi;
#endif
{
  mpz_t p, q, qdiv2;
#ifdef Q_MINUS_1
  mpz_t q_minus_1;
#endif
  mpz_ptr mtmp;
  register mpz_ptr pptr, qptr;
  register int retval = 1;
  register unsigned long int s;

  pptr = p;
  mpz_init_set (pptr, pi);
  qptr = q;
  mpz_init_set (qptr, qi);

#ifdef Q_MINUS_1
  mpz_init (q_minus_1);
#endif
  mpz_init (qdiv2);

tail_recurse2:
#ifdef DEBUG
  printf ("tail_recurse2: p=");
  mpz_out_str (stdout, 10, pptr);
  printf ("\nq=");
  mpz_out_str (stdout, 10, qptr);
  putchar ('\n');
#endif
  s = mpz_scan1 (qptr, 0);
  if (s) mpz_tdiv_q_2exp (qptr, qptr, s); /* J(a,2) = 1 */
#ifdef DEBUG
  printf ("2 factor decomposition: p=");
  mpz_out_str (stdout, 10, pptr);
  printf ("\nq=");
  mpz_out_str (stdout, 10, qptr);
  putchar ('\n');
#endif
  /* postcondition q odd */
  if (!mpz_cmp_ui (qptr, 1L))  /* J(a,1) = 1 */
    goto done;
  mpz_mod (pptr, pptr, qptr); /* J(a,q) = J(b,q) when a == b mod q */
#ifdef DEBUG
  printf ("mod out by q: p=");
  mpz_out_str (stdout, 10, pptr);
  printf ("\nq=");
  mpz_out_str (stdout, 10, qptr);
  putchar ('\n');
#endif
  /* quick calculation to get approximate size first */
  /* precondition: p < q */
  if ((mpz_sizeinbase (pptr, 2) + 1 >= mpz_sizeinbase (qptr,2))
      && (mpz_tdiv_q_2exp (qdiv2, qptr, 1L), mpz_cmp (pptr, qdiv2) > 0))
    {
      /* p > q/2 */
      mpz_sub (pptr, qptr, pptr);
      /* J(-1,q) = (-1)^((q-1)/2), q odd */
      if (mpz_get_ui (qptr) & 2)
	retval = -retval;
    }
  /* p < q/2 */
#ifdef Q_MINUS_1
  mpz_sub_ui (q_minus_q, qptr, 1L);
#endif
tail_recurse: /* we use tail_recurse only if q has not changed */
#ifdef DEBUG
  printf ("tail_recurse1: p=");
  mpz_out_str (stdout, 10, pptr);
  printf ("\nq=");
  mpz_out_str (stdout, 10, qptr);
  putchar ('\n');
#endif
  /*
   * J(0,q) = 0
   * this occurs only if gcd(p,q) != 1 which is never true for
   * Legendre function.
   */
  if (!mpz_cmp_ui (pptr, 0L))
    {
      retval = 0;
      goto done;
    }

  if (!mpz_cmp_ui (pptr, 1L))
    {
      /* J(1,q) = 1 */
      /* retval *= 1; */
      goto done;
    }
#ifdef Q_MINUS_1
  if (!mpz_cmp (pptr, q_minus_1))
    {
      /* J(-1,q) = (-1)^((q-1)/2) */
      if (mpz_get_ui (qptr) & 2)
	retval = -retval;
      /* else    retval *= 1; */
      goto done;
    }
#endif
  /*
   * we do not handle J(xy,q) except for x==2
   * since we do not want to factor
   */
  if ((s = mpz_scan1 (pptr, 0)) != 0)
    {
      /*
       * J(2,q) = (-1)^((q^2-1)/8)
       *
       * Note that q odd guarantees that q^2-1 is divisible by 8:
       * Let a: q=2a+1.  q^2 = 4a^2+4a+1, (q^2-1)/8 = a(a+1)/2, qed
       *
       * Now, note that this means that the low two bits of _a_
       * (or the low bits of q shifted over by 1 determines
       * the factor).
       */
      mpz_tdiv_q_2exp (pptr, pptr, s);

      /* even powers of 2 gives J(2,q)^{2n} = 1 */
      if (s & 1)
	{
	  s = mpz_get_ui (qptr) >> 1;
	  s = s * (s + 1);
	  if (s & 2)
	    retval = -retval;
	}
      goto tail_recurse;
    }
  /*
   * we know p is odd since we have cast out 2s
   * precondition that q is odd guarantees both odd.
   *
   * quadratic reciprocity
   * J(p,q) = (-1)^((p-1)(q-1)/4) * J(q,p)
   */
  if ((s = mpz_scan1 (pptr, 1)) <= 2 && (s + mpz_scan1 (qptr, 1)) <= 2)
    retval = -retval;

  mtmp = pptr; pptr = qptr; qptr = mtmp;
  goto tail_recurse2;
done:
  mpz_clear (p);
  mpz_clear (q);
  mpz_clear (qdiv2);
#ifdef Q_MINUS_1
  mpz_clear (q_minus_1);
#endif
  return retval;
}
