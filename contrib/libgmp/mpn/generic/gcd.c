/* mpn/gcd.c: mpn_gcd for gcd of two odd integers.

Copyright (C) 1991, 1993, 1994, 1995, 1996 Free Software Foundation, Inc.

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

/* Integer greatest common divisor of two unsigned integers, using
   the accelerated algorithm (see reference below).

   mp_size_t mpn_gcd (vp, vsize, up, usize).

   Preconditions [U = (up, usize) and V = (vp, vsize)]:

   1.  V is odd.
   2.  numbits(U) >= numbits(V).

   Both U and V are destroyed by the operation.  The result is left at vp,
   and its size is returned.

   Ken Weber (kweber@mat.ufrgs.br, kweber@mcs.kent.edu)

   Funding for this work has been partially provided by Conselho Nacional
   de Desenvolvimento Cienti'fico e Tecnolo'gico (CNPq) do Brazil, Grant
   301314194-2, and was done while I was a visiting reseacher in the Instituto
   de Matema'tica at Universidade Federal do Rio Grande do Sul (UFRGS).

   Refer to
	K. Weber, The accelerated integer GCD algorithm, ACM Transactions on
	Mathematical Software, v. 21 (March), 1995, pp. 111-122.  */

#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

/* If MIN (usize, vsize) > ACCEL_THRESHOLD, then the accelerated algorithm is
   used, otherwise the binary algorithm is used.  This may be adjusted for
   different architectures.  */
#ifndef ACCEL_THRESHOLD
#define ACCEL_THRESHOLD 4
#endif

/* When U and V differ in size by more than BMOD_THRESHOLD, the accelerated
   algorithm reduces using the bmod operation.  Otherwise, the k-ary reduction
   is used.  0 <= BMOD_THRESHOLD < BITS_PER_MP_LIMB.  */
enum
  {
    BMOD_THRESHOLD = BITS_PER_MP_LIMB/2
  };

#define SIGN_BIT  (~(~(mp_limb_t)0 >> 1))


#define SWAP_LIMB(UL, VL) do{mp_limb_t __l=(UL);(UL)=(VL);(VL)=__l;}while(0)
#define SWAP_PTR(UP, VP) do{mp_ptr __p=(UP);(UP)=(VP);(VP)=__p;}while(0)
#define SWAP_SZ(US, VS) do{mp_size_t __s=(US);(US)=(VS);(VS)=__s;}while(0)
#define SWAP_MPN(UP, US, VP, VS) do{SWAP_PTR(UP,VP);SWAP_SZ(US,VS);}while(0)

/* Use binary algorithm to compute V <-- GCD (V, U) for usize, vsize == 2.
   Both U and V must be odd.  */
static __gmp_inline mp_size_t
#if __STDC__
gcd_2 (mp_ptr vp, mp_srcptr up)
#else
gcd_2 (vp, up)
     mp_ptr vp;
     mp_srcptr up;
#endif
{
  mp_limb_t u0, u1, v0, v1;
  mp_size_t vsize;

  u0 = up[0], u1 = up[1], v0 = vp[0], v1 = vp[1];

  while (u1 != v1 && u0 != v0)
    {
      unsigned long int r;
      if (u1 > v1)
	{
	  u1 -= v1 + (u0 < v0), u0 -= v0;
	  count_trailing_zeros (r, u0);
	  u0 = u1 << (BITS_PER_MP_LIMB - r) | u0 >> r;
	  u1 >>= r;
	}
      else  /* u1 < v1.  */
	{
	  v1 -= u1 + (v0 < u0), v0 -= u0;
	  count_trailing_zeros (r, v0);
	  v0 = v1 << (BITS_PER_MP_LIMB - r) | v0 >> r;
	  v1 >>= r;
	}
    }

  vp[0] = v0, vp[1] = v1, vsize = 1 + (v1 != 0);

  /* If U == V == GCD, done.  Otherwise, compute GCD (V, |U - V|).  */
  if (u1 == v1 && u0 == v0)
    return vsize;

  v0 = (u0 == v0) ? (u1 > v1) ? u1-v1 : v1-u1 : (u0 > v0) ? u0-v0 : v0-u0;
  vp[0] = mpn_gcd_1 (vp, vsize, v0);

  return 1;
}

/* The function find_a finds 0 < N < 2^BITS_PER_MP_LIMB such that there exists
   0 < |D| < 2^BITS_PER_MP_LIMB, and N == D * C mod 2^(2*BITS_PER_MP_LIMB).
   In the reference article, D was computed along with N, but it is better to
   compute D separately as D <-- N / C mod 2^(BITS_PER_MP_LIMB + 1), treating
   the result as a twos' complement signed integer.

   Initialize N1 to C mod 2^(2*BITS_PER_MP_LIMB).  According to the reference
   article, N2 should be initialized to 2^(2*BITS_PER_MP_LIMB), but we use
   2^(2*BITS_PER_MP_LIMB) - N1 to start the calculations within double
   precision.  If N2 > N1 initially, the first iteration of the while loop
   will swap them.  In all other situations, N1 >= N2 is maintained.  */

static __gmp_inline mp_limb_t
#if __STDC__
find_a (mp_srcptr cp)
#else
find_a (cp)
     mp_srcptr cp;
#endif
{
  unsigned long int leading_zero_bits = 0;

  mp_limb_t n1_l = cp[0];	/* N1 == n1_h * 2^BITS_PER_MP_LIMB + n1_l.  */
  mp_limb_t n1_h = cp[1];

  mp_limb_t n2_l = -n1_l;	/* N2 == n2_h * 2^BITS_PER_MP_LIMB + n2_l.  */
  mp_limb_t n2_h = ~n1_h;

  /* Main loop.  */
  while (n2_h)			/* While N2 >= 2^BITS_PER_MP_LIMB.  */
    {
      /* N1 <-- N1 % N2.  */
      if ((SIGN_BIT >> leading_zero_bits & n2_h) == 0)
	{
	  unsigned long int i;
	  count_leading_zeros (i, n2_h);
	  i -= leading_zero_bits, leading_zero_bits += i;
	  n2_h = n2_h<<i | n2_l>>(BITS_PER_MP_LIMB - i), n2_l <<= i;
	  do
	    {
	      if (n1_h > n2_h || (n1_h == n2_h && n1_l >= n2_l))
		n1_h -= n2_h + (n1_l < n2_l), n1_l -= n2_l;
	      n2_l = n2_l>>1 | n2_h<<(BITS_PER_MP_LIMB - 1), n2_h >>= 1;
	      i -= 1;
	    }
	  while (i);
	}
      if (n1_h > n2_h || (n1_h == n2_h && n1_l >= n2_l))
	n1_h -= n2_h + (n1_l < n2_l), n1_l -= n2_l;

      SWAP_LIMB (n1_h, n2_h);
      SWAP_LIMB (n1_l, n2_l);
    }

  return n2_l;
}

mp_size_t
#if __STDC__
mpn_gcd (mp_ptr gp, mp_ptr vp, mp_size_t vsize, mp_ptr up, mp_size_t usize)
#else
mpn_gcd (gp, vp, vsize, up, usize)
     mp_ptr gp;
     mp_ptr vp;
     mp_size_t vsize;
     mp_ptr up;
     mp_size_t usize;
#endif
{
  mp_ptr orig_vp = vp;
  mp_size_t orig_vsize = vsize;
  int binary_gcd_ctr;		/* Number of times binary gcd will execute.  */
  TMP_DECL (marker);

  TMP_MARK (marker);

  /* Use accelerated algorithm if vsize is over ACCEL_THRESHOLD.
     Two EXTRA limbs for U and V are required for kary reduction.  */
  if (vsize > ACCEL_THRESHOLD)
    {
      unsigned long int vbitsize, d;
      mp_ptr orig_up = up;
      mp_size_t orig_usize = usize;
      mp_ptr anchor_up = (mp_ptr) TMP_ALLOC ((usize + 2) * BYTES_PER_MP_LIMB);

      MPN_COPY (anchor_up, orig_up, usize);
      up = anchor_up;

      count_leading_zeros (d, up[usize-1]);
      d = usize * BITS_PER_MP_LIMB - d;
      count_leading_zeros (vbitsize, vp[vsize-1]);
      vbitsize = vsize * BITS_PER_MP_LIMB - vbitsize;
      d = d - vbitsize + 1;

      /* Use bmod reduction to quickly discover whether V divides U.  */
      up[usize++] = 0;				/* Insert leading zero.  */
      mpn_bdivmod (up, up, usize, vp, vsize, d);

      /* Now skip U/V mod 2^d and any low zero limbs.  */
      d /= BITS_PER_MP_LIMB, up += d, usize -= d;
      while (usize != 0 && up[0] == 0)
	up++, usize--;

      if (usize == 0)				/* GCD == ORIG_V.  */
	goto done;

      vp = (mp_ptr) TMP_ALLOC ((vsize + 2) * BYTES_PER_MP_LIMB);
      MPN_COPY (vp, orig_vp, vsize);

      do					/* Main loop.  */
	{
	  if (up[usize-1] & SIGN_BIT)		/* U < 0; take twos' compl. */
	    {
	      mp_size_t i;
	      anchor_up[0] = -up[0];
	      for (i = 1; i < usize; i++)
		anchor_up[i] = ~up[i];
	      up = anchor_up;
	    }

	  MPN_NORMALIZE_NOT_ZERO (up, usize);

	  if ((up[0] & 1) == 0)			/* Result even; remove twos. */
	    {
	      unsigned long int r;
	      count_trailing_zeros (r, up[0]);
	      mpn_rshift (anchor_up, up, usize, r);
	      usize -= (anchor_up[usize-1] == 0);
	    }
	  else if (anchor_up != up)
	    MPN_COPY (anchor_up, up, usize);

	  SWAP_MPN (anchor_up, usize, vp, vsize);
	  up = anchor_up;

	  if (vsize <= 2)		/* Kary can't handle < 2 limbs and  */
	    break;			/* isn't efficient for == 2 limbs.  */

	  d = vbitsize;
	  count_leading_zeros (vbitsize, vp[vsize-1]);
	  vbitsize = vsize * BITS_PER_MP_LIMB - vbitsize;
	  d = d - vbitsize + 1;

	  if (d > BMOD_THRESHOLD)	/* Bmod reduction.  */
	    {
	      up[usize++] = 0;
	      mpn_bdivmod (up, up, usize, vp, vsize, d);
	      d /= BITS_PER_MP_LIMB, up += d, usize -= d;
	    }
	  else				/* Kary reduction.  */
	    {
	      mp_limb_t bp[2], cp[2];

	      /* C <-- V/U mod 2^(2*BITS_PER_MP_LIMB).  */
	      cp[0] = vp[0], cp[1] = vp[1];
	      mpn_bdivmod (cp, cp, 2, up, 2, 2*BITS_PER_MP_LIMB);

	      /* U <-- find_a (C)  *  U.  */
	      up[usize] = mpn_mul_1 (up, up, usize, find_a (cp));
	      usize++;

	      /* B <-- A/C == U/V mod 2^(BITS_PER_MP_LIMB + 1).
		  bp[0] <-- U/V mod 2^BITS_PER_MP_LIMB and
		  bp[1] <-- ( (U - bp[0] * V)/2^BITS_PER_MP_LIMB ) / V mod 2 */
	      bp[0] = up[0], bp[1] = up[1];
	      mpn_bdivmod (bp, bp, 2, vp, 2, BITS_PER_MP_LIMB);
	      bp[1] &= 1;	/* Since V is odd, division is unnecessary.  */

	      up[usize++] = 0;
	      if (bp[1])	/* B < 0: U <-- U + (-B)  * V.  */
		{
		   mp_limb_t c = mpn_addmul_1 (up, vp, vsize, -bp[0]);
		   mpn_add_1 (up + vsize, up + vsize, usize - vsize, c);
		}
	      else		/* B >= 0:  U <-- U - B * V.  */
		{
		  mp_limb_t b = mpn_submul_1 (up, vp, vsize, bp[0]);
		  mpn_sub_1 (up + vsize, up + vsize, usize - vsize, b);
		}

	      up += 2, usize -= 2;  /* At least two low limbs are zero.  */
	    }

	  /* Must remove low zero limbs before complementing.  */
	  while (usize != 0 && up[0] == 0)
	    up++, usize--;
	}
      while (usize);

      /* Compute GCD (ORIG_V, GCD (ORIG_U, V)).  Binary will execute twice.  */
      up = orig_up, usize = orig_usize;
      binary_gcd_ctr = 2;
    }
  else
    binary_gcd_ctr = 1;

  /* Finish up with the binary algorithm.  Executes once or twice.  */
  for ( ; binary_gcd_ctr--; up = orig_vp, usize = orig_vsize)
    {
      if (usize > 2)		/* First make U close to V in size.  */
	{
	  unsigned long int vbitsize, d;
	  count_leading_zeros (d, up[usize-1]);
	  d = usize * BITS_PER_MP_LIMB - d;
	  count_leading_zeros (vbitsize, vp[vsize-1]);
	  vbitsize = vsize * BITS_PER_MP_LIMB - vbitsize;
	  d = d - vbitsize - 1;
	  if (d != -(unsigned long int)1 && d > 2)
	    {
	      mpn_bdivmod (up, up, usize, vp, vsize, d);  /* Result > 0.  */
	      d /= (unsigned long int)BITS_PER_MP_LIMB, up += d, usize -= d;
	    }
	}

      /* Start binary GCD.  */
      do
	{
	  mp_size_t zeros;

	  /* Make sure U is odd.  */
	  MPN_NORMALIZE (up, usize);
	  while (up[0] == 0)
	    up += 1, usize -= 1;
	  if ((up[0] & 1) == 0)
	    {
	      unsigned long int r;
	      count_trailing_zeros (r, up[0]);
	      mpn_rshift (up, up, usize, r);
	      usize -= (up[usize-1] == 0);
	    }

	  /* Keep usize >= vsize.  */
	  if (usize < vsize)
	    SWAP_MPN (up, usize, vp, vsize);

	  if (usize <= 2)				/* Double precision. */
	    {
	      if (vsize == 1)
		vp[0] = mpn_gcd_1 (up, usize, vp[0]);
	      else
		vsize = gcd_2 (vp, up);
	      break;					/* Binary GCD done.  */
	    }

	  /* Count number of low zero limbs of U - V.  */
	  for (zeros = 0; up[zeros] == vp[zeros] && ++zeros != vsize; )
	    continue;

	  /* If U < V, swap U and V; in any case, subtract V from U.  */
	  if (zeros == vsize)				/* Subtract done.  */
	    up += zeros, usize -= zeros;
	  else if (usize == vsize)
	    {
	      mp_size_t size = vsize;
	      do
		size--;
	      while (up[size] == vp[size]);
	      if (up[size] < vp[size])			/* usize == vsize.  */
		SWAP_PTR (up, vp);
	      up += zeros, usize = size + 1 - zeros;
	      mpn_sub_n (up, up, vp + zeros, usize);
	    }
	  else
	    {
	      mp_size_t size = vsize - zeros;
	      up += zeros, usize -= zeros;
	      if (mpn_sub_n (up, up, vp + zeros, size))
		{
		  while (up[size] == 0)			/* Propagate borrow. */
		    up[size++] = -(mp_limb_t)1;
		  up[size] -= 1;
		}
	    }
	}
      while (usize);					/* End binary GCD.  */
    }

done:
  if (vp != gp)
    MPN_COPY (gp, vp, vsize);
  TMP_FREE (marker);
  return vsize;
}
