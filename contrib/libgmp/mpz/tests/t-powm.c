/* Test mpz_powm, mpz_mul. mpz_mod, mpz_mod_ui, mpz_div_ui.

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

void debug_mp ();

#ifndef SIZE
#define SIZE 8
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  mpz_t base, exp, mod;
  mpz_t r1, r2, t1, exp2, base2;
  mp_size_t base_size, exp_size, mod_size;
  int i;
  int reps = 10000;

  if (argc == 2)
     reps = atoi (argv[1]);

  mpz_init (base);
  mpz_init (exp);
  mpz_init (mod);
  mpz_init (r1);
  mpz_init (r2);
  mpz_init (t1);
  mpz_init (exp2);
  mpz_init (base2);

  for (i = 0; i < reps; i++)
    {
      base_size = urandom () % SIZE - SIZE/2;
      mpz_random2 (base, base_size);

      exp_size = urandom () % SIZE;
      mpz_random2 (exp, exp_size);

      mod_size = urandom () % SIZE /* - SIZE/2 */;
      mpz_random2 (mod, mod_size);
      if (mpz_cmp_ui (mod, 0) == 0)
	continue;

      /* This is mathematically undefined.  */
      if (mpz_cmp_ui (base, 0) == 0 && mpz_cmp_ui (exp, 0) == 0)
	continue;

#if 0
      putc ('\n', stderr);
      debug_mp (base, -16);
      debug_mp (exp, -16);
      debug_mp (mod, -16);
#endif

      mpz_powm (r1, base, exp, mod);

      mpz_set_ui (r2, 1);
      mpz_set (base2, base);
      mpz_set (exp2, exp);

      mpz_mod (r2, r2, mod);	/* needed when exp==0 and mod==1 */
      while (mpz_cmp_ui (exp2, 0) != 0)
	{
	  mpz_mod_ui (t1, exp2, 2);
	  if (mpz_cmp_ui (t1, 0) != 0)
	    {
	      mpz_mul (r2, r2, base2);
	      mpz_mod (r2, r2, mod);
	    }
	  mpz_mul (base2, base2, base2);
	  mpz_mod (base2, base2, mod);
	  mpz_div_ui (exp2, exp2, 2);
	}

#if 0
      debug_mp (r1, -16);
      debug_mp (r2, -16);
#endif

      if (mpz_cmp (r1, r2) != 0)
	abort ();
    }

  exit (0);
}

dump_abort (dividend, divisor)
     MP_INT *dividend, *divisor;
{
  fprintf (stderr, "ERROR\n");
  fprintf (stderr, "dividend = "); debug_mp (dividend, -16);
  fprintf (stderr, "divisor  = "); debug_mp (divisor, -16);
  abort();
}

void
debug_mp (x, base)
     MP_INT *x;
{
  mpz_out_str (stderr, base, x); fputc ('\n', stderr);
}
