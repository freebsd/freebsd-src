/* Test mpz_mul, mpz_divexact.

Copyright (C) 1996 Free Software Foundation, Inc.

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
#define SIZE 32
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  mpz_t op1, op2;
  mpz_t prod, quot;
  mp_size_t size;
  int i;
  int reps = 100000;

  if (argc == 2)
     reps = atoi (argv[1]);

  mpz_init (op1);
  mpz_init (op2);
  mpz_init (prod);
  mpz_init (quot);

  for (i = 0; i < reps; i++)
    {
      size = urandom () % SIZE - SIZE/2;
      mpz_random2 (op1, size);

      do
	{
	  size = urandom () % SIZE - SIZE/2;
	  mpz_random2 (op2, size);
	}
      while (mpz_cmp_ui (op2, 0) == 0);

      mpz_mul (prod, op1, op2);

      mpz_divexact (quot, prod, op2);
      if (mpz_cmp (quot, op1) != 0)
	dump_abort (quot, op1);
    }

  exit (0);
}

dump_abort (op1, op2)
     mpz_t op1, op2;
{
  fprintf (stderr, "ERROR\n");
  fprintf (stderr, "ref = "); debug_mp (op1, -16);
  fprintf (stderr, "wrong = "); debug_mp (op2, -16);
  abort();
}

void
debug_mp (x, base)
     mpz_t x;
{
  mpz_out_str (stderr, base, x); fputc ('\n', stderr);
}
