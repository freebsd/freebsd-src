/* mpn_mod_1(dividend_ptr, dividend_size, divisor_limb) --
   Divide (DIVIDEND_PTR,,DIVIDEND_SIZE) by DIVISOR_LIMB.
   Return the single-limb remainder.
   There are no constraints on the value of the divisor.

   QUOT_PTR and DIVIDEND_PTR might point to the same limb.

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

mp_limb
#ifdef __STDC__
mpn_mod_1 (mp_srcptr dividend_ptr, mp_size dividend_size,
	    unsigned long int divisor_limb)
#else
mpn_mod_1 (dividend_ptr, dividend_size, divisor_limb)
     mp_srcptr dividend_ptr;
     mp_size dividend_size;
     unsigned long int divisor_limb;
#endif
{
  int normalization_steps;
  mp_size i;
  mp_limb n1, n0, r;
  int dummy;

  /* Botch: Should this be handled at all?  Rely on callers?  */
  if (dividend_size == 0)
    return 0;

  if (UDIV_NEEDS_NORMALIZATION)
    {
      count_leading_zeros (normalization_steps, divisor_limb);
      if (normalization_steps != 0)
	{
	  divisor_limb <<= normalization_steps;

	  n1 = dividend_ptr[dividend_size - 1];
	  r = n1 >> (BITS_PER_MP_LIMB - normalization_steps);

	  /* Possible optimization:
	  if (r == 0
	      && divisor_limb > ((n1 << normalization_steps)
				 | (dividend_ptr[dividend_size - 2] >> ...)))
	    ...one division less...
	   */

	  for (i = dividend_size - 2; i >= 0; i--)
	    {
	      n0 = dividend_ptr[i];
	      udiv_qrnnd (dummy, r, r,
			  ((n1 << normalization_steps)
			   | (n0 >> (BITS_PER_MP_LIMB - normalization_steps))),
			  divisor_limb);
	      n1 = n0;
	    }
	  udiv_qrnnd (dummy, r, r,
		      n1 << normalization_steps,
		      divisor_limb);
	  return r >> normalization_steps;
	}
    }

  /* No normalization needed, either because udiv_qrnnd doesn't require
     it, or because DIVISOR_LIMB is already normalized.  */

  i = dividend_size - 1;
  r = dividend_ptr[i];

  if (r >= divisor_limb)
    {
      r = 0;
    }
  else
    {
      i--;
    }

  for (; i >= 0; i--)
    {
      n0 = dividend_ptr[i];
      udiv_qrnnd (dummy, r, r, n0, divisor_limb);
    }
  return r;
}
