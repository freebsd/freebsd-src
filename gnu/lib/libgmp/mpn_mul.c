/* mpn_mul -- Multiply two natural numbers.

Copyright (C) 1991, 1992 Free Software Foundation, Inc.

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

#ifdef DEBUG
#define MPN_MUL_VERIFY(res_ptr,res_size,op1_ptr,op1_size,op2_ptr,op2_size) \
  mpn_mul_verify (res_ptr, res_size, op1_ptr, op1_size, op2_ptr, op2_size)

#include <stdio.h>
static void
mpn_mul_verify (res_ptr, res_size, op1_ptr, op1_size, op2_ptr, op2_size)
     mp_ptr res_ptr, op1_ptr, op2_ptr;
     mp_size res_size, op1_size, op2_size;
{
  mp_ptr tmp_ptr;
  mp_size tmp_size;
  tmp_ptr = alloca ((op1_size + op2_size) * BYTES_PER_MP_LIMB);
  if (op1_size >= op2_size)
    tmp_size = mpn_mul_classic (tmp_ptr,
				 op1_ptr, op1_size, op2_ptr, op2_size);
  else
    tmp_size = mpn_mul_classic (tmp_ptr,
				 op2_ptr, op2_size, op1_ptr, op1_size);
  if (tmp_size != res_size
      || mpn_cmp (tmp_ptr, res_ptr, tmp_size) != 0)
    {
      fprintf (stderr, "GNU MP internal error: Wrong result in mpn_mul.\n");
      fprintf (stderr, "op1{%d} = ", op1_size); mpn_dump (op1_ptr, op1_size);
      fprintf (stderr, "op2{%d} = ", op2_size); mpn_dump (op2_ptr, op2_size);
      abort ();
    }
}
#else
#define MPN_MUL_VERIFY(a,b,c,d,e,f)
#endif

/* Multiply the natural numbers u (pointed to by UP, with USIZE limbs)
   and v (pointed to by VP, with VSIZE limbs), and store the result at
   PRODP.  USIZE + VSIZE limbs are always stored, but if the input
   operands are normalized, the return value will reflect the true
   result size (which is either USIZE + VSIZE, or USIZE + VSIZE -1).

   NOTE: The space pointed to by PRODP is overwritten before finished
   with U and V, so overlap is an error.

   Argument constraints:
   1. USIZE >= VSIZE.
   2. PRODP != UP and PRODP != VP, i.e. the destination
      must be distinct from the multiplier and the multiplicand.  */

/* If KARATSUBA_THRESHOLD is not already defined, define it to a
   value which is good on most machines.  */
#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD 8
#endif

/* The code can't handle KARATSUBA_THRESHOLD smaller than 4.  */
#if KARATSUBA_THRESHOLD < 4
#undef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD 4
#endif

mp_size
#ifdef __STDC__
mpn_mul (mp_ptr prodp,
	  mp_srcptr up, mp_size usize,
	  mp_srcptr vp, mp_size vsize)
#else
mpn_mul (prodp, up, usize, vp, vsize)
     mp_ptr prodp;
     mp_srcptr up;
     mp_size usize;
     mp_srcptr vp;
     mp_size vsize;
#endif
{
  mp_size n;
  mp_size prod_size;
  mp_limb cy;

  if (vsize < KARATSUBA_THRESHOLD)
    {
      /* Handle simple cases with traditional multiplication.

	 This is the most critical code of the entire function.  All
	 multiplies rely on this, both small and huge.  Small ones arrive
	 here immediately.  Huge ones arrive here as this is the base case
	 for the recursive algorithm below.  */
      mp_size i, j;
      mp_limb prod_low, prod_high;
      mp_limb cy_limb;
      mp_limb v_limb;

      if (vsize == 0)
	return 0;

      /* Offset UP and PRODP so that the inner loop can be faster.  */
      up += usize;
      prodp += usize;

      /* Multiply by the first limb in V separately, as the result can
	 be stored (not added) to PROD.  We also avoid a loop for zeroing.  */
      v_limb = vp[0];
      if (v_limb <= 1)
	{
	  if (v_limb == 1)
	    MPN_COPY (prodp - usize, up - usize, usize);
	  else
	    MPN_ZERO (prodp - usize, usize);
	  cy_limb = 0;
	}
      else
	{
	  cy_limb = 0;
	  j = -usize;
	  do
	    {
	      umul_ppmm (prod_high, prod_low, up[j], v_limb);
	      add_ssaaaa (cy_limb, prodp[j], prod_high, prod_low, 0, cy_limb);
	      j++;
	    }
	  while (j < 0);
	}

      prodp[0] = cy_limb;
      prodp++;

      /* For each iteration in the outer loop, multiply one limb from
	 U with one limb from V, and add it to PROD.  */
      for (i = 1; i < vsize; i++)
	{
	  v_limb = vp[i];
	  if (v_limb <= 1)
	    {
	      cy_limb = 0;
	      if (v_limb == 1)
		cy_limb = mpn_add (prodp - usize,
				    prodp - usize, usize, up - usize, usize);
	    }
	  else
	    {
	      cy_limb = 0;
	      j = -usize;

	      do
		{
		  umul_ppmm (prod_high, prod_low, up[j], v_limb);
		  add_ssaaaa (cy_limb, prod_low,
			      prod_high, prod_low, 0, cy_limb);
		  add_ssaaaa (cy_limb, prodp[j],
			      cy_limb, prod_low, 0, prodp[j]);
		  j++;
		}
	      while (j < 0);
	    }

	  prodp[0] = cy_limb;
	  prodp++;
	}

      return usize + vsize - (cy_limb == 0);
    }

  n = (usize + 1) / 2;

  /* Is USIZE larger than 1.5 times VSIZE?  Avoid Karatsuba's algorithm.  */
  if (2 * usize > 3 * vsize)
    {
      /* If U has at least twice as many limbs as V.  Split U in two
	 pieces, U1 and U0, such that U = U0 + U1*(2**BITS_PER_MP_LIMB)**N,
	 and recursively multiply the two pieces separately with V.  */

      mp_size u0_size;
      mp_ptr tmp;
      mp_size tmp_size;

      /* V1 (the high part of V) is zero.  */

      /* Calculate the length of U0.  It is normally equal to n, but
	 of course not for sure.  */
      for (u0_size = n; u0_size > 0 && up[u0_size - 1] == 0; u0_size--)
	;

      /* Perform (U0 * V).  */
      if (u0_size >= vsize)
	prod_size = mpn_mul (prodp, up, u0_size, vp, vsize);
      else
	prod_size = mpn_mul (prodp, vp, vsize, up, u0_size);
      MPN_MUL_VERIFY (prodp, prod_size, up, u0_size, vp, vsize);

      /* We have to zero-extend the lower partial product to n limbs,
	 since the mpn_add some lines below expect the first n limbs
	 to be well defined.  (This is normally a no-op.  It may
	 do something when U1 has many leading 0 limbs.) */
      while (prod_size < n)
	prodp[prod_size++] = 0;

      tmp = (mp_ptr) alloca ((usize + vsize - n) * BYTES_PER_MP_LIMB);

      /* Perform (U1 * V).  Make sure the first source argument to mpn_mul
	 is not less than the second source argument.  */
      if (vsize <= usize - n)
	tmp_size = mpn_mul (tmp, up + n, usize - n, vp, vsize);
      else
	tmp_size = mpn_mul (tmp, vp, vsize, up + n, usize - n);
      MPN_MUL_VERIFY (tmp, tmp_size, up + n, usize - n, vp, vsize);

      /* In this addition hides a potentially large copying of TMP.  */
      if (prod_size - n >= tmp_size)
	cy = mpn_add (prodp + n, prodp + n, prod_size - n, tmp, tmp_size);
      else
	cy = mpn_add (prodp + n, tmp, tmp_size, prodp + n, prod_size - n);
      if (cy)
	abort (); /* prodp[prod_size] = cy; */

      alloca (0);
      return tmp_size + n;
    }
  else
    {
      /* Karatsuba's divide-and-conquer algorithm.

	 Split U in two pieces, U1 and U0, such that
	 U = U0 + U1*(B**n),
	 and V in V1 and V0, such that
	 V = V0 + V1*(B**n).

	 UV is then computed recursively using the identity

		2n   n        n                   n
	 UV = (B  + B )U V + B (U -U )(V -V ) + (B + 1)U V
                        1 1      1  0   0  1            0 0

	 Where B = 2**BITS_PER_MP_LIMB.
       */

      /* It's possible to decrease the temporary allocation by using the
	 prodp area for temporary storage of the middle term, and doing
	 that recursive multiplication first.  (Do this later.)  */

      mp_size u0_size;
      mp_size v0_size;
      mp_size u0v0_size;
      mp_size u1v1_size;
      mp_ptr temp;
      mp_size temp_size;
      mp_size utem_size;
      mp_size vtem_size;
      mp_ptr ptem;
      mp_size ptem_size;
      int negflg;
      mp_ptr pp;

      pp = (mp_ptr) alloca (4 * n * BYTES_PER_MP_LIMB);

      /* Calculate the lengths of U0 and V0.  They are normally equal
	 to n, but of course not for sure.  */
      for (u0_size = n; u0_size > 0 && up[u0_size - 1] == 0; u0_size--)
	;
      for (v0_size = n; v0_size > 0 && vp[v0_size - 1] == 0; v0_size--)
	;

      /*** 1. PROD]2n..0] := U0 x V0
	    (Recursive call to mpn_mul may NOT overwrite input operands.)
	     ________________  ________________
	    |________________||____U0 x V0_____|  */

      if (u0_size >= v0_size)
	u0v0_size = mpn_mul (pp, up, u0_size, vp, v0_size);
      else
	u0v0_size = mpn_mul (pp, vp, v0_size, up, u0_size);
      MPN_MUL_VERIFY (pp, u0v0_size, up, u0_size, vp, v0_size);

      /* Zero-extend to 2n limbs. */
      while (u0v0_size < 2 * n)
	pp[u0v0_size++] = 0;


      /*** 2. PROD]4n..2n] := U1 x V1
	    (Recursive call to mpn_mul may NOT overwrite input operands.)
	     ________________  ________________
	    |_____U1 x V1____||____U0 x V0_____|  */

      u1v1_size = mpn_mul (pp + 2*n,
			     up + n, usize - n,
			     vp + n, vsize - n);
      MPN_MUL_VERIFY (pp + 2*n, u1v1_size,
		      up + n, usize - n, vp + n, vsize - n);
      prod_size = 2 * n + u1v1_size;


      /*** 3. PTEM]2n..0] := (U1-U0) x (V0-V1)
	    (Recursive call to mpn_mul may overwrite input operands.)
	     ________________
	    |_(U1-U0)(V0-V1)_|  */

      temp = (mp_ptr) alloca ((2 * n + 1) * BYTES_PER_MP_LIMB);
      if (usize - n > u0_size
	  || (usize - n == u0_size
	      && mpn_cmp (up + n, up, u0_size) >= 0))
	{
	  utem_size = usize - n
	    + mpn_sub (temp, up + n, usize - n, up, u0_size);
	  negflg = 0;
	}
      else
	{
	  utem_size = u0_size
	    + mpn_sub (temp, up, u0_size, up + n, usize - n);
	  negflg = 1;
	}
      if (vsize - n > v0_size
	  || (vsize - n == v0_size
	      && mpn_cmp (vp + n, vp, v0_size) >= 0))
	{
	  vtem_size = vsize - n
	    + mpn_sub (temp + n, vp + n, vsize - n, vp, v0_size);
	  negflg ^= 1;
	}
      else
	{
	  vtem_size = v0_size
	    + mpn_sub (temp + n, vp, v0_size, vp + n, vsize - n);
	  /* No change of NEGFLG.  */
	}
      ptem = (mp_ptr) alloca (2 * n * BYTES_PER_MP_LIMB);
      if (utem_size >= vtem_size)
	ptem_size = mpn_mul (ptem, temp, utem_size, temp + n, vtem_size);
      else
	ptem_size = mpn_mul (ptem, temp + n, vtem_size, temp, utem_size);
      MPN_MUL_VERIFY (ptem, ptem_size, temp, utem_size, temp + n, vtem_size);

      /*** 4. TEMP]2n..0] := PROD]2n..0] + PROD]4n..2n]
	      ________________
	     |_____U1 x V1____|
	      ________________
	     |_____U0_x_V0____|  */

      cy = mpn_add (temp, pp, 2*n, pp + 2*n, u1v1_size);
      if (cy != 0)
	{
	  temp[2*n] = cy;
	  temp_size = 2*n + 1;
	}
      else
	{
	  /* Normalize temp.  pp[2*n-1] might have been zero in the
	     mpn_add call above, and thus temp might be unnormalized.  */
	  for (temp_size = 2*n; temp_size > 0 && temp[temp_size - 1] == 0;
	       temp_size--)
	    ;
	}

      if (prod_size - n >= temp_size)
	cy = mpn_add (pp + n, pp + n, prod_size - n, temp, temp_size);
      else
	{
	  /* This is a weird special case that should not happen (often)!  */
	  cy = mpn_add (pp + n, temp, temp_size, pp + n, prod_size - n);
	  prod_size = temp_size + n;
	}
      if (cy != 0)
	{
	  pp[prod_size] = cy;
	  prod_size++;
	}
#ifdef DEBUG
      if (prod_size > 4 * n)
	abort();
#endif
      if (negflg)
	prod_size = prod_size
	  + mpn_sub (pp + n, pp + n, prod_size - n, ptem, ptem_size);
      else
	{
	  if (prod_size - n < ptem_size)
	    abort();
	  cy = mpn_add (pp + n, pp + n, prod_size - n, ptem, ptem_size);
	  if (cy != 0)
	    {
	      pp[prod_size] = cy;
	      prod_size++;
#ifdef DEBUG
	      if (prod_size > 4 * n)
		abort();
#endif
	    }
	}

      MPN_COPY (prodp, pp, prod_size);
      alloca (0);
      return prod_size;
    }
}
