/* Test mpz_add, mpz_cmp, mpz_cmp_ui, mpz_gcd, mpz_gcdext, mpz_get_str,
   mpz_mod, mpz_mul, mpz_set_str.

Copyright (C) 1993 Free Software Foundation, Inc.

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

void debug_mp ();

#ifndef SIZE
#define SIZE 8
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  MP_INT op1, op2;
  mp_size size;
  int i;
  int reps = 100000;
  char *str;
  int base;

  if (argc == 2)
     reps = atoi (argv[1]);

  mpz_init (&op1);
  mpz_init (&op2);

  for (i = 0; i < reps; i++)
    {
      size = urandom () % SIZE - SIZE/2;

      mpz_random2 (&op1, size);
      base = urandom () % 36 + 1;
      if (base == 1)
	base = 0;

      str = mpz_get_str ((char *) 0, base, &op1);
      mpz_set_str (&op2, str, base);
      free (str);

      if (mpz_cmp (&op1, &op2))
	{
	  fprintf (stderr, "ERROR\n");
	  fprintf (stderr, "op1  = "); debug_mp (&op1, -16);
	  fprintf (stderr, "base = %d\n", base);
	  abort ();
	}
    }

  exit (0);
}

void
debug_mp (x, base)
     MP_INT *x;
{
  mpz_out_str (stderr, base, x); fputc ('\n', stderr);
}
