/* Test mpz_gcd, mpz_gcdext, mpz_mul, mpz_tdiv_r, mpz_add, mpz_cmp,
   mpz_cmp_ui, mpz_init_set, mpz_set, mpz_clear.

Copyright (C) 1991, 1993, 1994, 1996 Free Software Foundation, Inc.

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

void mpz_refgcd (), debug_mp ();

#ifndef SIZE
#define SIZE 256 /* really needs to be this large to exercise corner cases! */
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  mpz_t op1, op2;
  mpz_t refgcd, gcd, s, t;
  mpz_t temp1, temp2;
  mp_size_t op1_size, op2_size;
  int i;
  int reps = 1000;

  if (argc == 2)
     reps = atoi (argv[1]);

  mpz_init (op1);
  mpz_init (op2);
  mpz_init (refgcd);
  mpz_init (gcd);
  mpz_init (temp1);
  mpz_init (temp2);
  mpz_init (s);
  mpz_init (t);

  for (i = 0; i < reps; i++)
    {
      op1_size = urandom () % SIZE - SIZE/2;
      op2_size = urandom () % SIZE - SIZE/2;

      mpz_random2 (op1, op1_size);
      mpz_random2 (op2, op2_size);

      mpz_refgcd (refgcd, op1, op2);

      mpz_gcd (gcd, op1, op2);
      if (mpz_cmp (refgcd, gcd))
	dump_abort (op1, op2);

      mpz_gcdext (gcd, s, t, op1, op2);
      if (mpz_cmp (refgcd, gcd))
	dump_abort (op1, op2);

      mpz_mul (temp1, s, op1);
      mpz_mul (temp2, t, op2);
      mpz_add (gcd, temp1, temp2);
      if (mpz_cmp (refgcd, gcd))
	dump_abort (op1, op2);
    }

  exit (0);
}

void
mpz_refgcd (g, x, y)
     mpz_t g;
     mpz_t x, y;
{
  mpz_t xx, yy;

  mpz_init (xx);
  mpz_init (yy);

  mpz_abs (xx, x);
  mpz_abs (yy, y);

  for (;;)
    {
      if (mpz_cmp_ui (yy, 0) == 0)
	{
	  mpz_set (g, xx);
	  break;
	}
      mpz_tdiv_r (xx, xx, yy);
      if (mpz_cmp_ui (xx, 0) == 0)
	{
	  mpz_set (g, yy);
	  break;
	}
      mpz_tdiv_r (yy, yy, xx);
    }

  mpz_clear (xx);
  mpz_clear (yy);
}

dump_abort (op1, op2)
     mpz_t op1, op2;
{
  fprintf (stderr, "ERROR\n");
  fprintf (stderr, "op1 = "); debug_mp (op1, -16);
  fprintf (stderr, "op2 = "); debug_mp (op2, -16);
  abort();
}

void
debug_mp (x, base)
     mpz_t x;
{
  mpz_out_str (stderr, base, x); fputc ('\n', stderr);
}
