/* mpz/gcd.c:   Calculate the greatest common divisor of two integers.

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

#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

void *_mpz_realloc ();

#ifndef BERKELEY_MP
void
#if __STDC__
mpz_gcd (mpz_ptr g, mpz_srcptr u, mpz_srcptr v)
#else
mpz_gcd (g, u, v)
     mpz_ptr g;
     mpz_srcptr u;
     mpz_srcptr v;
#endif
#else /* BERKELEY_MP */
void
#if __STDC__
gcd (mpz_srcptr u, mpz_srcptr v, mpz_ptr g)
#else
gcd (u, v, g)
     mpz_ptr g;
     mpz_srcptr u;
     mpz_srcptr v;
#endif
#endif /* BERKELEY_MP */

{
  unsigned long int g_zero_bits, u_zero_bits, v_zero_bits;
  mp_size_t g_zero_limbs, u_zero_limbs, v_zero_limbs;
  mp_ptr tp;
  mp_ptr up = u->_mp_d;
  mp_size_t usize = ABS (u->_mp_size);
  mp_ptr vp = v->_mp_d;
  mp_size_t vsize = ABS (v->_mp_size);
  mp_size_t gsize;
  TMP_DECL (marker);

  /* GCD(0, V) == V.  */
  if (usize == 0)
    {
      g->_mp_size = vsize;
      if (g == v)
	return;
      if (g->_mp_alloc < vsize)
	_mpz_realloc (g, vsize);
      MPN_COPY (g->_mp_d, vp, vsize);
      return;
    }

  /* GCD(U, 0) == U.  */
  if (vsize == 0)
    {
      g->_mp_size = usize;
      if (g == u)
	return;
      if (g->_mp_alloc < usize)
	_mpz_realloc (g, usize);
      MPN_COPY (g->_mp_d, up, usize);
      return;
    }

  if (usize == 1)
    {
      g->_mp_size = 1;
      g->_mp_d[0] = mpn_gcd_1 (vp, vsize, up[0]);
      return;
    }

  if (vsize == 1)
    {
      g->_mp_size = 1;
      g->_mp_d[0] = mpn_gcd_1 (up, usize, vp[0]);
      return;
    }

  TMP_MARK (marker);

  /*  Eliminate low zero bits from U and V and move to temporary storage.  */
  while (*up == 0)
    up++;
  u_zero_limbs = up - u->_mp_d;
  usize -= u_zero_limbs;
  count_trailing_zeros (u_zero_bits, *up);
  tp = up;
  up = (mp_ptr) TMP_ALLOC (usize * BYTES_PER_MP_LIMB);
  if (u_zero_bits != 0)
    {
      mpn_rshift (up, tp, usize, u_zero_bits);
      usize -= up[usize - 1] == 0;
    }
  else
    MPN_COPY (up, tp, usize);

  while (*vp == 0)
    vp++;
  v_zero_limbs = vp - v->_mp_d;
  vsize -= v_zero_limbs;
  count_trailing_zeros (v_zero_bits, *vp);
  tp = vp;
  vp = (mp_ptr) TMP_ALLOC (vsize * BYTES_PER_MP_LIMB);
  if (v_zero_bits != 0)
    {
      mpn_rshift (vp, tp, vsize, v_zero_bits);
      vsize -= vp[vsize - 1] == 0;
    }
  else
    MPN_COPY (vp, tp, vsize);

  if (u_zero_limbs > v_zero_limbs)
    {
      g_zero_limbs = v_zero_limbs;
      g_zero_bits = v_zero_bits;
    }
  else if (u_zero_limbs < v_zero_limbs)
    {
      g_zero_limbs = u_zero_limbs;
      g_zero_bits = u_zero_bits;
    }
  else  /*  Equal.  */
    {
      g_zero_limbs = u_zero_limbs;
      g_zero_bits = MIN (u_zero_bits, v_zero_bits);
    }

  /*  Call mpn_gcd.  The 1st argument must not have more bits than the 2nd.  */
  vsize = (usize < vsize || (usize == vsize && up[usize-1] < vp[vsize-1]))
    ? mpn_gcd (vp, up, usize, vp, vsize)
    : mpn_gcd (vp, vp, vsize, up, usize);

  /*  Here G <-- V << (g_zero_limbs*BITS_PER_MP_LIMB + g_zero_bits).  */
  gsize = vsize + g_zero_limbs;
  if (g_zero_bits != 0)
    {
      mp_limb_t cy_limb;
      gsize += (vp[vsize - 1] >> (BITS_PER_MP_LIMB - g_zero_bits)) != 0;
      if (g->_mp_alloc < gsize)
	_mpz_realloc (g, gsize);
      MPN_ZERO (g->_mp_d, g_zero_limbs);

      tp = g->_mp_d + g_zero_limbs;
      cy_limb = mpn_lshift (tp, vp, vsize, g_zero_bits);
      if (cy_limb != 0)
	tp[vsize] = cy_limb;
    }
  else
    {
      if (g->_mp_alloc < gsize)
	_mpz_realloc (g, gsize);
      MPN_ZERO (g->_mp_d, g_zero_limbs);
      MPN_COPY (g->_mp_d + g_zero_limbs, vp, vsize);
    }

  g->_mp_size = gsize;
  TMP_FREE (marker);
}
