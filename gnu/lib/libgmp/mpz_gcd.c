/* mpz_gcd -- Calculate the greatest common divisior of two integers.

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
#include "longlong.h"

#ifndef BERKELEY_MP
void
#ifdef __STDC__
mpz_gcd (MP_INT *w, const MP_INT *u, const MP_INT *v)
#else
mpz_gcd (w, u, v)
     MP_INT *w;
     const MP_INT *u;
     const MP_INT *v;
#endif
#else /* BERKELEY_MP */
void
#ifdef __STDC__
gcd (const MP_INT *u, const MP_INT *v, MP_INT *w)
#else
gcd (u, v, w)
     const MP_INT *u;
     const MP_INT *v;
     MP_INT *w;
#endif
#endif /* BERKELEY_MP */
{
  mp_size usize, vsize, wsize;
  mp_ptr up_in, vp_in;
  mp_ptr up, vp;
  mp_ptr wp;
  mp_size i;
  mp_limb d;
  int bcnt;
  mp_size w_bcnt;
  mp_limb cy_digit;

  usize = ABS (u->size);
  vsize = ABS (v->size);

  /* GCD(0,v) == v.  */
  if (usize == 0)
    {
      if (w->alloc < vsize)
	_mpz_realloc (w, vsize);

      w->size = vsize;
      MPN_COPY (w->d, v->d, vsize);
      return;
    }

  /* GCD(0,u) == u.  */
  if (vsize == 0)
    {
      if (w->alloc < usize)
	_mpz_realloc (w, usize);

      w->size = usize;
      MPN_COPY (w->d, u->d, usize);
      return;
    }

  /* Make U odd by shifting it down as many bit positions as there
     are zero bits.  Put the result in temporary space.  */
  up = (mp_ptr) alloca (usize * BYTES_PER_MP_LIMB);
  up_in = u->d;
  for (i = 0; (d = up_in[i]) == 0; i++)
    ;
  count_leading_zeros (bcnt, d & -d);
  bcnt = BITS_PER_MP_LIMB - 1 - bcnt;
  usize = mpn_rshift (up, up_in + i, usize - i, bcnt);

  bcnt += i * BITS_PER_MP_LIMB;
  w_bcnt = bcnt;

  /* Make V odd by shifting it down as many bit positions as there
     are zero bits.  Put the result in temporary space.  */
  vp = (mp_ptr) alloca (vsize * BYTES_PER_MP_LIMB);
  vp_in = v->d;
  for (i = 0; (d = vp_in[i]) == 0; i++)
    ;
  count_leading_zeros (bcnt, d & -d);
  bcnt = BITS_PER_MP_LIMB - 1 - bcnt;
  vsize = mpn_rshift (vp, vp_in + i, vsize - i, bcnt);

  /* W_BCNT is set to the minimum of the number of zero bits in U and V.
     Thus it represents the number of common 2 factors.  */
  bcnt += i * BITS_PER_MP_LIMB;
  if (bcnt < w_bcnt)
    w_bcnt = bcnt;

  for (;;)
    {
      int cmp;

      cmp = usize - vsize != 0 ? usize - vsize : mpn_cmp (up, vp, usize);

      /* If U and V have become equal, we have found the GCD.  */
      if (cmp == 0)
	break;

      if (cmp > 0)
	{
	  /* Replace U by (U - V) >> cnt, with cnt being the least value
	     making U odd again.  */

	  usize += mpn_sub (up, up, usize, vp, vsize);
	  for (i = 0; (d = up[i]) == 0; i++)
	    ;
	  count_leading_zeros (bcnt, d & -d);
	  bcnt = BITS_PER_MP_LIMB - 1 - bcnt;
	  usize = mpn_rshift (up, up + i, usize - i, bcnt);
	}
      else
	{
	  /* Replace V by (V - U) >> cnt, with cnt being the least value
	     making V odd again.  */

	  vsize += mpn_sub (vp, vp, vsize, up, usize);
	  for (i = 0; (d = vp[i]) == 0; i++)
	    ;
	  count_leading_zeros (bcnt, d & -d);
	  bcnt = BITS_PER_MP_LIMB - 1 - bcnt;
	  vsize = mpn_rshift (vp, vp + i, vsize - i, bcnt);
	}
    }

  /* GCD(U_IN, V_IN) now is U * 2**W_BCNT.  */

  wsize = usize + w_bcnt / BITS_PER_MP_LIMB + 1;
  if (w->alloc < wsize)
    _mpz_realloc (w, wsize);

  wp = w->d;

  MPN_ZERO (wp, w_bcnt / BITS_PER_MP_LIMB);

  cy_digit = mpn_lshift (wp + w_bcnt / BITS_PER_MP_LIMB, up, usize,
			  w_bcnt % BITS_PER_MP_LIMB);
  wsize = usize + w_bcnt / BITS_PER_MP_LIMB;
  if (cy_digit != 0)
    {
      wp[wsize] = cy_digit;
      wsize++;
    }

  w->size = wsize;

  alloca (0);
}
