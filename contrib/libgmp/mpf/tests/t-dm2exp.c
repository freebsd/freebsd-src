/* Test mpf_div, mpf_div_2exp, mpf_mul_2exp.

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

#ifndef SIZE
#define SIZE 16
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  int reps = 500000;
  int i;
  mpf_t u, v, w1, w2;
  mp_size_t bprec = 100;
  mpf_t rerr, limit_rerr;

  if (argc > 1)
    {
      reps = strtol (argv[1], 0, 0);
      if (argc > 2)
	bprec = strtol (argv[2], 0, 0);
    }

  mpf_set_default_prec (bprec);

  mpf_init (rerr);
  mpf_init (limit_rerr);

  mpf_init (u);
  mpf_init (v);
  mpf_init (w1);
  mpf_init (w2);

  for (i = 0; i < reps; i++)
    {
      mp_size_t res_prec;
      unsigned long int pow2;

      res_prec = urandom () % (bprec + 100);
      mpf_set_prec (w1, res_prec);
      mpf_set_prec (w2, res_prec);

      mpf_set_ui (limit_rerr, 1);
      mpf_div_2exp (limit_rerr, limit_rerr, res_prec);

      pow2 = urandom () % 0x10000;
      mpf_set_ui (v, 1);
      mpf_mul_2exp (v, v, pow2);

      mpf_div_2exp (w1, u, pow2);
      mpf_div (w2, u, v);
      mpf_reldiff (rerr, w1, w2);
      if (mpf_cmp (rerr, limit_rerr) > 0)
	{
	  printf ("ERROR in mpf_div or mpf_div_2exp after %d tests\n", i);
	  printf ("   u = "); mpf_dump (u);
	  printf ("   v = "); mpf_dump (v);
	  printf ("  w1 = "); mpf_dump (w1);
	  printf ("  w2 = "); mpf_dump (w2);
	  abort ();
	}
    }

  exit (0);
}

oo (x)
     mpf_t x;
{
  mp_size_t i;
  printf (" exp = %ld\n", x->_mp_exp);
  printf ("size = %d\n", x->_mp_size);
  for (i = ABS (x->_mp_size) - 1; i >= 0; i--)
    printf ("%08lX ", x->_mp_d[i]);
  printf ("\n");
  mpf_dump (x);
}
