/* Test mpf_get_str and mpf_set_str.

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
#define SIZE 10
#endif

#ifndef EXPO
#define EXPO 20
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  mpf_t x, y;
  int reps = 20000;
  int i;
  mp_size_t bprec = 100;
  mpf_t d, rerr, max_rerr, limit_rerr;
  char *str;
  long bexp;
  long size, exp;
  int base;
  char buf[SIZE * BITS_PER_MP_LIMB + 5];

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

  mpf_init (x);
  mpf_init (y);
  mpf_init (d);

  for (i = 0; i < reps; i++)
    {
      size = urandom () % (2 * SIZE) - SIZE;
      exp = urandom () % EXPO;
      mpf_random2 (x, size, exp);

      base = urandom () % 35 + 2;

      str = mpf_get_str (0, &bexp, base, 0, x);

      if (str[0] == '-')
	sprintf (buf, "-0.%s@%ld", str + 1, bexp);
      else
	sprintf (buf, "0.%s@%ld", str, bexp);

      mpf_set_str (y, buf, -base);
      free (str);

      mpf_reldiff (rerr, x, y);
      if (mpf_cmp (rerr, max_rerr) > 0)
	{
	  mpf_set (max_rerr, rerr);
#if VERBOSE
	  mpf_dump (max_rerr);
#endif
	  if (mpf_cmp (rerr, limit_rerr) > 0)
	    {
	      printf ("ERROR after %d tests\n", i);
	      printf ("base = %d\n", base);
	      printf ("   x = "); mpf_dump (x);
	      printf ("   y = "); mpf_dump (y);
	      abort ();
	    }
	}
    }

  exit (0);
}

oo (x)
     mpf_t x;
{
  int i;
  printf (" exp = %ld\n", x->_mp_exp);
  printf ("size = %d\n", x->_mp_size);
  for (i = ABS (x->_mp_size) - 1; i >= 0; i--)
    printf ("%08lX ", x->_mp_d[i]);
  printf ("\n");
  mpf_dump (x);
}
