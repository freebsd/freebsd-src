/* mpz_gcdext(g, s, t, a, b) -- Set G to gcd(a, b), and S and T such that
   g = as + bt.

Copyright (C) 1991 Free Software Foundation, Inc.

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

#include "gmp.h"
#include "gmp-impl.h"

/* Botch:  SLOW!  */

void
#ifdef __STDC__
mpz_gcdext (MP_INT *g, MP_INT *s, MP_INT *t, const MP_INT *a, const MP_INT *b)
#else
mpz_gcdext (g, s, t, a, b)
     MP_INT *g;
     MP_INT *s;
     MP_INT *t;
     const MP_INT *a;
     const MP_INT *b;
#endif
{
  MP_INT s0, s1, q, r, x, d0, d1;

  mpz_init_set_ui (&s0, 1);
  mpz_init_set_ui (&s1, 0);
  mpz_init (&q);
  mpz_init (&r);
  mpz_init (&x);
  mpz_init_set (&d0, a);
  mpz_init_set (&d1, b);

  while (d1.size != 0)
    {
      mpz_divmod (&q, &r, &d0, &d1);
      mpz_set (&d0, &d1);
      mpz_set (&d1, &r);

      mpz_mul (&x, &s1, &q);
      mpz_sub (&x, &s0, &x);
      mpz_set (&s0, &s1);
      mpz_set (&s1, &x);
    }

  if (t != NULL)
    {
      mpz_mul (&x, &s0, a);
      mpz_sub (&x, &d0, &x);
      if (b->size == 0)
	t->size = 0;
      else
	mpz_div (t, &x, b);
    }
  mpz_set (s, &s0);
  mpz_set (g, &d0);

  mpz_clear (&s0);
  mpz_clear (&s1);
  mpz_clear (&q);
  mpz_clear (&r);
  mpz_clear (&x);
  mpz_clear (&d0);
  mpz_clear (&d1);
}
