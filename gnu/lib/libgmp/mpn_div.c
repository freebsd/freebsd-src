/* mpn_div -- Divide natural numbers, producing both remainder and
   quotient.

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

/* Divide num (NUM_PTR/NUM_SIZE) by den (DEN_PTR/DEN_SIZE) and write
   the quotient at QUOT_PTR and the remainder at NUM_PTR.

   Return 0 or 1, depending on if the quotient size is (NSIZE - DSIZE)
   or (NSIZE - DSIZE + 1).

   Argument constraints:
   1. The most significant bit of d must be set.
   2. QUOT_PTR != DEN_PTR and QUOT_PTR != NUM_PTR, i.e. the quotient storage
      area must be distinct from either input operands.

   The exact sizes of the quotient and remainder must be determined
   by the caller, in spite of the return value.  The return value just
   informs the caller about if the highest digit is written or not, and
   it may very well be 0.  */

/* THIS WILL BE IMPROVED SOON.  MORE COMMENTS AND FASTER CODE.  */

mp_size
#ifdef __STDC__
mpn_div (mp_ptr quot_ptr,
	 mp_ptr num_ptr, mp_size num_size,
	 mp_srcptr den_ptr, mp_size den_size)
#else
mpn_div (quot_ptr, num_ptr, num_size, den_ptr, den_size)
     mp_ptr quot_ptr;
     mp_ptr num_ptr;
     mp_size num_size;
     mp_srcptr den_ptr;
     mp_size den_size;
#endif
{
  mp_size q_is_long = 0;

  switch (den_size)
    {
    case 0:
      /* We are asked to divide by zero, so go ahead and do it!
	 (To make the compiler not remove this statement, assign NUM_SIZE
	 and fall through.)  */
      num_size = 1 / den_size;

    case 1:
      {
	mp_size i;
	mp_limb n1, n0;
	mp_limb d;

	d = den_ptr[0];
	i = num_size - 1;
	n1 = num_ptr[i];
	i--;

	if (n1 >= d)
	  {
	    q_is_long = 1;
	    n1 = 0;
	    i++;
	  }

	for (; i >= 0; i--)
	  {
	    n0 = num_ptr[i];
	    udiv_qrnnd (quot_ptr[i], n1, n1, n0, d);
	  }

	num_ptr[0] = n1;
      }
      break;

    case 2:
      {
	mp_size i;
	mp_limb n0, n1, n2;
	mp_limb d0, d1;

	num_ptr += num_size - 2;
	d0 = den_ptr[1];
	d1 = den_ptr[0];
	n0 = num_ptr[1];
	n1 = num_ptr[0];

	if (n0 >= d0)
	  {
	    q_is_long = 1;
	    n1 = n0;
	    n0 = 0;
	    num_ptr++;
	    num_size++;
	  }

	for (i = num_size - den_size - 1; i >= 0; i--)
	  {
	    mp_limb q;
	    mp_limb r;

	    num_ptr--;
	    if (n0 == d0)
	      {
		/* Q should be either 111..111 or 111..110.  Need special
		   treatment of this rare case as normal division would
		   give overflow.  */
		q = ~0;

		r = n1 + d0;
		if (r < d0)	/* Carry in the addition? */
		  {
		    n2 = num_ptr[0];

		    add_ssaaaa (n0, n1, r - d1, n2, 0, d1);
		    quot_ptr[i] = q;
		    continue;
		  }
		n0 = d1 - (d1 != 0);
		n1 = -d1;
	      }
	    else
	      {
		udiv_qrnnd (q, r, n0, n1, d0);
		umul_ppmm (n0, n1, d1, q);
	      }

	    n2 = num_ptr[0];
	  q_test:
	    if (n0 > r || (n0 == r && n1 > n2))
	      {
		/* The estimated Q was too large.  */
		q--;

		sub_ddmmss (n0, n1, n0, n1, 0, d1);
		r += d0;
		if (r >= d0)	/* If not carry, test q again.  */
		  goto q_test;
	      }

	    quot_ptr[i] = q;
	    sub_ddmmss (n0, n1, r, n2, n0, n1);
	  }
	num_ptr[1] = n0;
	num_ptr[0] = n1;
      }
      break;

    default:
      {
	mp_size i;
	mp_limb d0 = den_ptr[den_size - 1];
	mp_limb d1 = den_ptr[den_size - 2];
	mp_limb n0 = num_ptr[num_size - 1];
	int ugly_hack_flag = 0;

	if (n0 >= d0)
	  {

	    /* There's a problem with this case, which shows up later in the
	       code.  q becomes 1 (and sometimes 0) the first time when
	       we've been here, and w_cy == 0 after the main do-loops below.
	       But c = num_ptr[j] reads rubbish outside the num_ptr vector!
	       Maybe I can solve this cleanly when I fix the early-end
	       optimization here in the default case.  For now, I change the
	       add_back entering condition, to kludge.  Leaving the stray
	       memref behind!

	       HACK: Added ugly_hack_flag to make it work.  */

	    q_is_long = 1;
	    n0 = 0;
	    num_size++;
	    ugly_hack_flag = 1;
	  }

	num_ptr += num_size;
	den_ptr += den_size;
	for (i = num_size - den_size - 1; i >= 0; i--)
	  {
	    mp_limb q;
	    mp_limb n1;
	    mp_limb w_cy;
	    mp_limb d, c;
	    mp_size j;

	    num_ptr--;
	    if (n0 == d0)
	      /* This might over-estimate q, but it's probably not worth
		 the extra code here to find out.  */
	      q = ~0;
	    else
	      {
		mp_limb r;

		udiv_qrnnd (q, r, n0, num_ptr[-1], d0);
		umul_ppmm (n1, n0, d1, q);

		while (n1 > r || (n1 == r && n0 > num_ptr[-2]))
		  {
		    q--;
		    r += d0;
		    if (r < d0)	/* I.e. "carry in previous addition?"  */
		      break;
		    n1 -= n0 < d1;
		    n0 -= d1;
		  }
	      }

	    w_cy = 0;
	    j = -den_size;
	    do
	      {
		d = den_ptr[j];
		c = num_ptr[j];
		umul_ppmm (n1, n0, d, q);
		n0 += w_cy;
		w_cy = (n0 < w_cy) + n1;
		n0 = c - n0;
		num_ptr[j] = n0;
		if (n0 > c)
		  goto cy_loop;
	      ncy_loop:
		j++;
	      }
	    while  (j < 0);

	    if (ugly_hack_flag)
	      {
		c = 0;
		ugly_hack_flag = 0;
	      }
	    else
	      c = num_ptr[j];
	    if (c >= w_cy)
	      goto store_q;
	    goto add_back;

	    do
	      {
		d = den_ptr[j];
		c = num_ptr[j];
		umul_ppmm (n1, n0, d, q);
		n0 += w_cy;
		w_cy = (n0 < w_cy) + n1;
		n0 = c - n0 - 1;
		num_ptr[j] = n0;
		if (n0 < c)
		  goto ncy_loop;
	      cy_loop:
		j++;
	      }
	    while  (j < 0);

	    if (ugly_hack_flag)
	      {
		c = 0;
		ugly_hack_flag = 0;
	      }
	    else
	      c = num_ptr[j];
	    w_cy++;
	    if (c >= w_cy)
	      goto store_q;

	  add_back:
	    j = -den_size;
	    do
	      {
		d = den_ptr[j];
		n0 = num_ptr[j] + d;
		num_ptr[j] = n0;
		if (n0 < d)
		  goto ab_cy_loop;
	      ab_ncy_loop:
		j++;
	      }
	    while  (j < 0);
	    abort ();		/* We should always have a carry out! */

	    do
	      {
		d = den_ptr[j];
		n0 = num_ptr[j] + d + 1;
		num_ptr[j] = n0;
		if (n0 > d)
		  goto ab_ncy_loop;
	      ab_cy_loop:
		j++;
	      }
	    while  (j < 0);
	    q--;

	  store_q:
	    quot_ptr[i] = q;
	  }
      }
    }

  return q_is_long;
}
