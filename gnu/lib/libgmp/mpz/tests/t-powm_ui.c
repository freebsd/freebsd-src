/* Test mpz_powm_ui, mpz_mul. mpz_mod, mpz_mod_ui, mpz_div_ui.

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
  mpz_t base, mod;
  mpz_t r1, r2, base2;
  mp_size_t base_size, mod_size;
  mp_limb_t exp, exp2;
  int i;
  int reps = 10000;

  if (argc == 2)
     reps = atoi (argv[1]);

  mpz_init (base);
  mpz_init (mod);
  mpz_init (r1);
  mpz_init (r2);
  mpz_init (base2);

  for (i = 0; i < reps; i++)
    {
      base_size = urandom () % SIZE /* - SIZE/2 */;
      mpz_random2 (base, base_size);

      mpn_random2 (&exp, 1);

      mod_size = urandom () % SIZE /* - SIZE/2 */;
      mpz_random2 (mod, mod_size);
      if (mpz_cmp_ui (mod, 0) == 0)
	continue;

      /* This is mathematically undefined.  */
      if (mpz_cmp_ui (base, 0) == 0 && exp == 0)
	continue;

#if 0
      putc ('\n', stderr);
      debug_mp (base, -16);
      debug_mp (mod, -16);
#endif

      mpz_powm_ui (r1, base, (unsigned long int) exp, mod);

      mpz_set_ui (r2, 1);
      mpz_set (base2, base);
      exp2 = exp;

      mpz_mod (r2, r2, mod);	/* needed when exp==0 and mod==1 */
      while (exp2 != 0)
	{
	  if (exp2 % 2 != 0)
	    {
	      mpz_mul (r2, r2, base2);
	      mpz_mod (r2, r2, mod);
	    }
	  mpz_mul (base2, base2, base2);
	  mpz_mod (base2, base2, mod);
	  exp2 = exp2 / 2;
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
