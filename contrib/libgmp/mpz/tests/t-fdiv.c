/* Test mpz_abs, mpz_add, mpz_cmp, mpz_cmp_ui, mpz_fdiv_qr, mpz_fdiv_q,
   mpz_fdiv_r, mpz_mul.

Copyright (C) 1993, 1994, 1996 Free Software Foundation, Inc.

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

#include <stdio.h>
#include "gmp.h"
#include "gmp-impl.h"
#include "urandom.h"

void debug_mp ();

#ifndef SIZE
#define SIZE 16
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  mpz_t dividend, divisor;
  mpz_t quotient, remainder;
  mpz_t quotient2, remainder2;
  mpz_t temp;
  mp_size_t dividend_size, divisor_size;
  int i;
  int reps = 100000;

  if (argc == 2)
     reps = atoi (argv[1]);

  mpz_init (dividend);
  mpz_init (divisor);
  mpz_init (quotient);
  mpz_init (remainder);
  mpz_init (quotient2);
  mpz_init (remainder2);
  mpz_init (temp);

  for (i = 0; i < reps; i++)
    {
      dividend_size = urandom () % SIZE - SIZE/2;
      mpz_random2 (dividend, dividend_size);

      divisor_size = urandom () % SIZE - SIZE/2;
      mpz_random2 (divisor, divisor_size);
      if (mpz_cmp_ui (divisor, 0) == 0)
	continue;

      mpz_fdiv_qr (quotient, remainder, dividend, divisor);
      mpz_fdiv_q (quotient2, dividend, divisor);
      mpz_fdiv_r (remainder2, dividend, divisor);

      /* First determine that the quotients and remainders computed
	 with different functions are equal.  */
      if (mpz_cmp (quotient, quotient2) != 0)
	dump_abort (dividend, divisor);
      if (mpz_cmp (remainder, remainder2) != 0)
	dump_abort (dividend, divisor);

      /* Check if the sign of the quotient is correct.  */
      if (mpz_cmp_ui (quotient, 0) != 0)
	if ((mpz_cmp_ui (quotient, 0) < 0)
	    != ((mpz_cmp_ui (dividend, 0) ^ mpz_cmp_ui (divisor, 0)) < 0))
	dump_abort (dividend, divisor);

      /* Check if the remainder has the same sign as the divisor
	 (quotient rounded towards minus infinity).  */
      if (mpz_cmp_ui (remainder, 0) != 0)
	if ((mpz_cmp_ui (remainder, 0) < 0) != (mpz_cmp_ui (divisor, 0) < 0))
	  dump_abort (dividend, divisor);

      mpz_mul (temp, quotient, divisor);
      mpz_add (temp, temp, remainder);
      if (mpz_cmp (temp, dividend) != 0)
	dump_abort (dividend, divisor);

      mpz_abs (temp, divisor);
      mpz_abs (remainder, remainder);
      if (mpz_cmp (remainder, temp) >= 0)
	dump_abort (dividend, divisor);
    }

  exit (0);
}

dump_abort (dividend, divisor)
     mpz_t dividend, divisor;
{
  fprintf (stderr, "ERROR\n");
  fprintf (stderr, "dividend = "); debug_mp (dividend, -16);
  fprintf (stderr, "divisor  = "); debug_mp (divisor, -16);
  abort();
}

void
debug_mp (x, base)
     mpz_t x;
{
  mpz_out_str (stderr, base, x); fputc ('\n', stderr);
}
