/* mpz_gcdext(g, s, t, a, b) -- Set G to gcd(a, b), and S and T such that
   g = as + bt.

Copyright (C) 1991, 1993, 1994, 1995 Free Software Foundation, Inc.

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

#include "gmp.h"
#include "gmp-impl.h"

/* Botch:  SLOW!  */

void
#if __STDC__
mpz_gcdext (mpz_ptr g, mpz_ptr s, mpz_ptr t, mpz_srcptr a, mpz_srcptr b)
#else
mpz_gcdext (g, s, t, a, b)
     mpz_ptr g;
     mpz_ptr s;
     mpz_ptr t;
     mpz_srcptr a;
     mpz_srcptr b;
#endif
{
  mpz_t s0, s1, q, r, x, d0, d1;

  mpz_init_set_ui (s0, 1L);
  mpz_init_set_ui (s1, 0L);
  mpz_init (q);
  mpz_init (r);
  mpz_init (x);
  mpz_init_set (d0, a);
  mpz_init_set (d1, b);

  while (d1->_mp_size != 0)
    {
      mpz_tdiv_qr (q, r, d0, d1);
      mpz_set (d0, d1);
      mpz_set (d1, r);

      mpz_mul (x, s1, q);
      mpz_sub (x, s0, x);
      mpz_set (s0, s1);
      mpz_set (s1, x);
    }

  if (t != NULL)
    {
      mpz_mul (x, s0, a);
      mpz_sub (x, d0, x);
      if (b->_mp_size == 0)
	t->_mp_size = 0;
      else
	mpz_tdiv_q (t, x, b);
    }
  mpz_set (s, s0);
  mpz_set (g, d0);
  if (g->_mp_size < 0)
    {
      g->_mp_size = -g->_mp_size;
      s->_mp_size = -s->_mp_size;
      if (t != NULL)
	t->_mp_size = -t->_mp_size;
    }

  mpz_clear (s0);
  mpz_clear (s1);
  mpz_clear (q);
  mpz_clear (r);
  mpz_clear (x);
  mpz_clear (d0);
  mpz_clear (d1);
}
