/* Test mpz_gcd, mpz_gcdext, mpz_mul, mpz_mod, mpz_add, mpz_cmp,
   mpz_cmp_ui. mpz_init_set, mpz_set, mpz_clear.

Copyright (C) 1991, 1993 Free Software Foundation, Inc.

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

#include <stdio.h>
#include "gmp.h"
#include "gmp-impl.h"
#include "urandom.h"

void mpz_refgcd (), debug_mp ();

#ifndef SIZE
#define SIZE 8
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  MP_INT op1, op2;
  MP_INT refgcd, gcd, s, t;
  MP_INT temp1, temp2;
  mp_size op1_size, op2_size;
  int i;
  int reps = 10000;

  if (argc == 2)
     reps = atoi (argv[1]);

  mpz_init (&op1);
  mpz_init (&op2);
  mpz_init (&refgcd);
  mpz_init (&gcd);
  mpz_init (&temp1);
  mpz_init (&temp2);
  mpz_init (&s);
  mpz_init (&t);

  for (i = 0; i < reps; i++)
    {
      op1_size = urandom () % SIZE;
      op2_size = urandom () % SIZE;

      mpz_random2 (&op1, op1_size);
      mpz_random2 (&op2, op2_size);

      mpz_refgcd (&refgcd, &op1, &op2);

      mpz_gcd (&gcd, &op1, &op2);
      if (mpz_cmp (&refgcd, &gcd))
	dump_abort (&op1, &op2);

      mpz_gcdext (&gcd, &s, &t, &op1, &op2);
      if (mpz_cmp (&refgcd, &gcd))
	dump_abort (&op1, &op2);

      mpz_mul (&temp1, &s, &op1);
      mpz_mul (&temp2, &t, &op2);
      mpz_add (&gcd, &temp1, &temp2);
      if (mpz_cmp (&refgcd, &gcd))
	dump_abort (&op1, &op2);
    }

  exit (0);
}

void
mpz_refgcd (g, x, y)
     MP_INT *g;
     MP_INT *x, *y;
{
  MP_INT xx, yy;

  mpz_init (&xx);
  mpz_init (&yy);

  mpz_abs (&xx, x);
  mpz_abs (&yy, y);

  for (;;)
    {
      if (mpz_cmp_ui (&yy, 0) == 0)
	{
	  mpz_set (g, &xx);
	  break;
	}
      mpz_mod (&xx, &xx, &yy);
      if (mpz_cmp_ui (&xx, 0) == 0)
	{
	  mpz_set (g, &yy);
	  break;
	}
      mpz_mod (&yy, &yy, &xx);
    }

  mpz_clear (&xx);
  mpz_clear (&yy);
}

dump_abort (op1, op2)
     MP_INT *op1, *op2;
{
  fprintf (stderr, "ERROR\n");
  fprintf (stderr, "op1 = "); debug_mp (op1, -16);
  fprintf (stderr, "op2 = "); debug_mp (op2, -16);
  abort();
}

void
debug_mp (x, base)
     MP_INT *x;
{
  mpz_out_str (stderr, base, x); fputc ('\n', stderr);
}
