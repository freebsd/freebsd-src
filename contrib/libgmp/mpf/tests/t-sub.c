/* Test mpf_sub.

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

#if __STDC__
void ref_mpf_add (mpf_t, const mpf_t, const mpf_t);
void ref_mpf_sub (mpf_t, const mpf_t, const mpf_t);
#else
void ref_mpf_add ();
void ref_mpf_sub ();
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  mp_size_t size;
  mp_exp_t exp;
  int reps = 500000;
  int i;
  mpf_t u, v, w, wref;
  mp_size_t bprec = 100;
  mpf_t rerr, max_rerr, limit_rerr;

  if (argc > 1)
    {
      reps = strtol (argv[1], 0, 0);
      if (argc > 2)
	bprec = strtol (argv[2], 0, 0);
    }

  mpf_set_default_prec (bprec);

  mpf_init_set_ui (limit_rerr, 1);
  mpf_div_2exp (limit_rerr, limit_rerr, bprec);
#if VERBOSE
  mpf_dump (limit_rerr);
#endif
  mpf_init (rerr);
  mpf_init_set_ui (max_rerr, 0);

  mpf_init (u);
  mpf_init (v);
  mpf_init (w);
  mpf_init (wref);
  for (i = 0; i < reps; i++)
    {
      size = urandom () % (2 * SIZE) - SIZE;
      exp = urandom () % SIZE;
      mpf_random2 (u, size, exp);

      size = urandom () % (2 * SIZE) - SIZE;
      exp = urandom () % SIZE;
      mpf_random2 (v, size, exp);

      if ((urandom () & 1) != 0)
	mpf_add_ui (u, v, 1);
      else if ((urandom () & 1) != 0)
	mpf_sub_ui (u, v, 1);

      mpf_sub (w, u, v);
      ref_mpf_sub (wref, u, v);

      mpf_reldiff (rerr, w, wref);
      if (mpf_cmp (rerr, max_rerr) > 0)
	{
	  mpf_set (max_rerr, rerr);
#if VERBOSE
	  mpf_dump (max_rerr);
#endif
	  if (mpf_cmp (rerr, limit_rerr) > 0)
	    {
	      printf ("ERROR after %d tests\n", i);
	      printf ("   u = "); mpf_dump (u);
	      printf ("   v = "); mpf_dump (v);
	      printf ("wref = "); mpf_dump (wref);
	      printf ("   w = "); mpf_dump (w);
	      abort ();
	    }
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
