/* mpn_gcdext -- Extended Greatest Common Divisor.

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

#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

#ifndef EXTEND
#define EXTEND 1
#endif

#if STAT
int arr[BITS_PER_MP_LIMB];
#endif

#define SGN(A) (((A) < 0) ? -1 : ((A) > 0))

/* Idea 1: After we have performed a full division, don't shift operands back,
	   but instead account for the extra factors-of-2 thus introduced.
   Idea 2: Simple generalization to use divide-and-conquer would give us an
	   algorithm that runs faster than O(n^2).
   Idea 3: The input numbers need less space as the computation progresses,
	   while the s0 and s1 variables need more space.  To save space, we
	   could make them share space, and have the latter variables grow
	   into the former.  */

/* Precondition: U >= V.  */

mp_size_t
#if EXTEND
#if __STDC__
mpn_gcdext (mp_ptr gp, mp_ptr s0p,
	    mp_ptr up, mp_size_t size, mp_ptr vp, mp_size_t vsize)
#else
mpn_gcdext (gp, s0p, up, size, vp, vsize)
     mp_ptr gp;
     mp_ptr s0p;
     mp_ptr up;
     mp_size_t size;
     mp_ptr vp;
     mp_size_t vsize;
#endif
#else
#if __STDC__
mpn_gcd (mp_ptr gp,
	 mp_ptr up, mp_size_t size, mp_ptr vp, mp_size_t vsize)
#else
mpn_gcd (gp, up, size, vp, vsize)
     mp_ptr gp;
     mp_ptr up;
     mp_size_t size;
     mp_ptr vp;
     mp_size_t vsize;
#endif
#endif
{
  mp_limb_t uh, vh;
  mp_limb_signed_t A, B, C, D;
  int cnt;
  mp_ptr tp, wp;
#if RECORD
  mp_limb_signed_t min = 0, max = 0;
#endif
#if EXTEND
  mp_ptr s1p;
  mp_ptr orig_s0p = s0p;
  mp_size_t ssize, orig_size = size;
  TMP_DECL (mark);

  TMP_MARK (mark);

  tp = (mp_ptr) TMP_ALLOC ((size + 1) * BYTES_PER_MP_LIMB);
  wp = (mp_ptr) TMP_ALLOC ((size + 1) * BYTES_PER_MP_LIMB);
  s1p = (mp_ptr) TMP_ALLOC (size * BYTES_PER_MP_LIMB);

  MPN_ZERO (s0p, size);
  MPN_ZERO (s1p, size);

  s0p[0] = 1;
  s1p[0] = 0;
  ssize = 1;
#endif

  if (size > vsize)
    {
      /* Normalize V (and shift up U the same amount).  */
      count_leading_zeros (cnt, vp[vsize - 1]);
      if (cnt != 0)
	{
	  mp_limb_t cy;
	  mpn_lshift (vp, vp, vsize, cnt);
	  cy = mpn_lshift (up, up, size, cnt);
	  up[size] = cy;
	  size += cy != 0;
	}

      mpn_divmod (up + vsize, up, size, vp, vsize);
#if EXTEND
      /* This is really what it boils down to in this case... */
      s0p[0] = 0;
      s1p[0] = 1;
#endif
      size = vsize;
      if (cnt != 0)
	{
	  mpn_rshift (up, up, size, cnt);
	  mpn_rshift (vp, vp, size, cnt);
	}
      {
	mp_ptr xp;
	xp = up; up = vp; vp = xp;
      }
    }

  for (;;)
    {
      /* Figure out exact size of V.  */
      vsize = size;
      MPN_NORMALIZE (vp, vsize);
      if (vsize <= 1)
	break;

      /* Make UH be the most significant limb of U, and make VH be
	 corresponding bits from V.  */
      uh = up[size - 1];
      vh = vp[size - 1];
      count_leading_zeros (cnt, uh);
      if (cnt != 0)
	{
	  uh = (uh << cnt) | (up[size - 2] >> (BITS_PER_MP_LIMB - cnt));
	  vh = (vh << cnt) | (vp[size - 2] >> (BITS_PER_MP_LIMB - cnt));
	}

#if 0
      /* For now, only handle BITS_PER_MP_LIMB-1 bits.  This makes
	 room for sign bit.  */
      uh >>= 1;
      vh >>= 1;
#endif
      A = 1;
      B = 0;
      C = 0;
      D = 1;

      for (;;)
	{
	  mp_limb_signed_t q, T;
	  if (vh + C == 0 || vh + D == 0)
	    break;

	  q = (uh + A) / (vh + C);
	  if (q != (uh + B) / (vh + D))
	    break;

	  T = A - q * C;
	  A = C;
	  C = T;
	  T = B - q * D;
	  B = D;
	  D = T;
	  T = uh - q * vh;
	  uh = vh;
	  vh = T;
	}

#if RECORD
      min = MIN (A, min);  min = MIN (B, min);
      min = MIN (C, min);  min = MIN (D, min);
      max = MAX (A, max);  max = MAX (B, max);
      max = MAX (C, max);  max = MAX (D, max);
#endif

      if (B == 0)
	{
	  mp_limb_t qh;
	  mp_size_t i;

	  /* This is quite rare.  I.e., optimize something else!  */

	  /* Normalize V (and shift up U the same amount).  */
	  count_leading_zeros (cnt, vp[vsize - 1]);
	  if (cnt != 0)
	    {
	      mp_limb_t cy;
	      mpn_lshift (vp, vp, vsize, cnt);
	      cy = mpn_lshift (up, up, size, cnt);
	      up[size] = cy;
	      size += cy != 0;
	    }

	  qh = mpn_divmod (up + vsize, up, size, vp, vsize);
#if EXTEND
	  MPN_COPY (tp, s0p, ssize);
	  for (i = 0; i < size - vsize; i++)
	    {
	      mp_limb_t cy;
	      cy = mpn_addmul_1 (tp + i, s1p, ssize, up[vsize + i]);
	      if (cy != 0)
		tp[ssize++] = cy;
	    }
	  if (qh != 0)
	    {
	      mp_limb_t cy;
	      abort ();
	      /* XXX since qh == 1, mpn_addmul_1 is overkill */
	      cy = mpn_addmul_1 (tp + size - vsize, s1p, ssize, qh);
	      if (cy != 0)
		tp[ssize++] = cy;
      	    }
#if 0
	  MPN_COPY (s0p, s1p, ssize); /* should be old ssize, kind of */
	  MPN_COPY (s1p, tp, ssize);
#else
	  {
	    mp_ptr xp;
	    xp = s0p; s0p = s1p; s1p = xp;
	    xp = s1p; s1p = tp; tp = xp;
	  }
#endif
#endif
	  size = vsize;
	  if (cnt != 0)
	    {
	      mpn_rshift (up, up, size, cnt);
	      mpn_rshift (vp, vp, size, cnt);
	    }

	  {
	    mp_ptr xp;
	    xp = up; up = vp; vp = xp;
	  }
	  MPN_NORMALIZE (up, size);
	}
      else
	{
	  /* T = U*A + V*B
	     W = U*C + V*D
	     U = T
	     V = W	   */

	  if (SGN(A) == SGN(B))	/* should be different sign */
	    abort ();
	  if (SGN(C) == SGN(D))	/* should be different sign */
	    abort ();
#if STAT
	  { mp_limb_t x;
	    x = ABS (A) | ABS (B) | ABS (C) | ABS (D);
	    count_leading_zeros (cnt, x);
	    arr[BITS_PER_MP_LIMB - cnt]++; }
#endif
	  if (A == 0)
	    {
	      if (B != 1) abort ();
	      MPN_COPY (tp, vp, size);
	    }
	  else
	    {
	      if (A < 0)
		{
		  mpn_mul_1 (tp, vp, size, B);
		  mpn_submul_1 (tp, up, size, -A);
		}
	      else
		{
		  mpn_mul_1 (tp, up, size, A);
		  mpn_submul_1 (tp, vp, size, -B);
		}
	    }
	  if (C < 0)
	    {
	      mpn_mul_1 (wp, vp, size, D);
	      mpn_submul_1 (wp, up, size, -C);
	    }
	  else
	    {
	      mpn_mul_1 (wp, up, size, C);
	      mpn_submul_1 (wp, vp, size, -D);
	    }

	  {
	    mp_ptr xp;
	    xp = tp; tp = up; up = xp;
	    xp = wp; wp = vp; vp = xp;
	  }

#if EXTEND
	  { mp_limb_t cy;
	  MPN_ZERO (tp, orig_size);
	  if (A == 0)
	    {
	      if (B != 1) abort ();
	      MPN_COPY (tp, s1p, ssize);
	    }
	  else
	    {
	      if (A < 0)
		{
		  cy = mpn_mul_1 (tp, s1p, ssize, B);
		  cy += mpn_addmul_1 (tp, s0p, ssize, -A);
		}
	      else
		{
		  cy = mpn_mul_1 (tp, s0p, ssize, A);
		  cy += mpn_addmul_1 (tp, s1p, ssize, -B);
		}
	      if (cy != 0)
		tp[ssize++] = cy;
	    }
	  MPN_ZERO (wp, orig_size);
	  if (C < 0)
	    {
	      cy = mpn_mul_1 (wp, s1p, ssize, D);
	      cy += mpn_addmul_1 (wp, s0p, ssize, -C);
	    }
	  else
	    {
	      cy = mpn_mul_1 (wp, s0p, ssize, C);
	      cy += mpn_addmul_1 (wp, s1p, ssize, -D);
	    }
	  if (cy != 0)
	    wp[ssize++] = cy;
	  }
	  {
	    mp_ptr xp;
	    xp = tp; tp = s0p; s0p = xp;
	    xp = wp; wp = s1p; s1p = xp;
	  }
#endif
#if 0	/* Is it a win to remove multiple zeros here? */
	  MPN_NORMALIZE (up, size);
#else
	  if (up[size - 1] == 0)
	    size--;
#endif
	}
    }

#if RECORD
  printf ("min: %ld\n", min);
  printf ("max: %ld\n", max);
#endif

  if (vsize == 0)
    {
      if (gp != up)
	MPN_COPY (gp, up, size);
#if EXTEND
      if (orig_s0p != s0p)
	MPN_COPY (orig_s0p, s0p, ssize);
#endif
      TMP_FREE (mark);
      return size;
    }
  else
    {
      mp_limb_t vl, ul, t;
#if EXTEND
      mp_limb_t cy;
      mp_size_t i;
#endif
      vl = vp[0];
#if EXTEND
      t = mpn_divmod_1 (wp, up, size, vl);
      MPN_COPY (tp, s0p, ssize);
      for (i = 0; i < size; i++)
	{
	  cy = mpn_addmul_1 (tp + i, s1p, ssize, wp[i]);
	  if (cy != 0)
	    tp[ssize++] = cy;
	}
#if 0
      MPN_COPY (s0p, s1p, ssize);
      MPN_COPY (s1p, tp, ssize);
#else
      {
	mp_ptr xp;
	xp = s0p; s0p = s1p; s1p = xp;
	xp = s1p; s1p = tp; tp = xp;
      }
#endif
#else
      t = mpn_mod_1 (up, size, vl);
#endif
      ul = vl;
      vl = t;
      while (vl != 0)
	{
	  mp_limb_t t;
#if EXTEND
	  mp_limb_t q, cy;
	  q = ul / vl;
	  t = ul - q*vl;

	  MPN_COPY (tp, s0p, ssize);
	  cy = mpn_addmul_1 (tp, s1p, ssize, q);
	  if (cy != 0)
	    tp[ssize++] = cy;
#if 0
	  MPN_COPY (s0p, s1p, ssize);
	  MPN_COPY (s1p, tp, ssize);
#else
	  {
	    mp_ptr xp;
	    xp = s0p; s0p = s1p; s1p = xp;
	    xp = s1p; s1p = tp; tp = xp;
	  }
#endif

#else
	  t = ul % vl;
#endif
	  ul = vl;
	  vl = t;
	}
      gp[0] = ul;
#if EXTEND
      if (orig_s0p != s0p)
	MPN_COPY (orig_s0p, s0p, ssize);
#endif
      TMP_FREE (mark);
      return 1;
    }
}
