/* mpz_probab_prime_p --
   An implementation of the probabilistic primality test found in Knuth's
   Seminumerical Algorithms book.  If the function mpz_probab_prime_p()
   returns 0 then n is not prime.  If it returns 1, then n is 'probably'
   prime.  The probability of a false positive is (1/4)**reps, where
   reps is the number of internal passes of the probabilistic algorithm.
   Knuth indicates that 25 passes are reasonable.

Copyright (C) 1991, 1993, 1994 Free Software Foundation, Inc.
Contributed by John Amanatides.

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

static int
possibly_prime (n, n_minus_1, x, y, q, k)
     mpz_srcptr n;
     mpz_srcptr n_minus_1;
     mpz_ptr x;
     mpz_ptr y;
     mpz_srcptr q;
     unsigned long int k;
{
  unsigned long int i;

  /* find random x s.t. 1 < x < n */
  do
    {
      mpz_random (x, mpz_size (n));
      mpz_mmod (x, x, n);
    }
  while (mpz_cmp_ui (x, 1L) <= 0);

  mpz_powm (y, x, q, n);

  if (mpz_cmp_ui (y, 1L) == 0 || mpz_cmp (y, n_minus_1) == 0)
    return 1;

  for (i = 1; i < k; i++)
    {
      mpz_powm_ui (y, y, 2L, n);
      if (mpz_cmp (y, n_minus_1) == 0)
	return 1;
      if (mpz_cmp_ui (y, 1L) == 0)
	return 0;
    }
  return 0;
}

int
#if __STDC__
mpz_probab_prime_p (mpz_srcptr m, int reps)
#else
mpz_probab_prime_p (m, reps)
     mpz_srcptr m;
     int reps;
#endif
{
  mpz_t n, n_minus_1, x, y, q;
  int i, is_prime;
  unsigned long int k;

  mpz_init (n);
  /* Take the absolute value of M, to handle positive and negative primes.  */
  mpz_abs (n, m);

  if (mpz_cmp_ui (n, 3L) <= 0)
    {
      mpz_clear (n);
      return mpz_cmp_ui (n, 1L) > 0;
    }

  if ((mpz_get_ui (n) & 1) == 0)
    {
      mpz_clear (n);
      return 0;			/* even */
    }

  mpz_init (n_minus_1);
  mpz_sub_ui (n_minus_1, n, 1L);
  mpz_init (x);
  mpz_init (y);

  /* find q and k, s.t.  n = 1 + 2**k * q */
  mpz_init_set (q, n_minus_1);
  k = mpz_scan1 (q, 0);
  mpz_tdiv_q_2exp (q, q, k);

  is_prime = 1;
  for (i = 0; i < reps && is_prime; i++)
    is_prime &= possibly_prime (n, n_minus_1, x, y, q, k);

  mpz_clear (n_minus_1);
  mpz_clear (n);
  mpz_clear (x);
  mpz_clear (y);
  mpz_clear (q);
  return is_prime;
}
