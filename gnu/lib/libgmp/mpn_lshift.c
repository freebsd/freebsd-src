/* mpn_lshift -- Shift left low level.

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

/* Shift U (pointed to by UP and USIZE digits long) CNT bits to the left
   and store the USIZE least significant digits of the result at WP.
   Return the bits shifted out from the most significant digit.

   Argument constraints:
   0. U must be normalized (i.e. it's most significant digit != 0).
   1. 0 <= CNT < BITS_PER_MP_LIMB
   2. If the result is to be written over the input, WP must be >= UP.
*/

mp_limb
#ifdef __STDC__
mpn_lshift (mp_ptr wp,
	    mp_srcptr up, mp_size usize,
	    unsigned cnt)
#else
mpn_lshift (wp, up, usize, cnt)
     mp_ptr wp;
     mp_srcptr up;
     mp_size usize;
     unsigned cnt;
#endif
{
  mp_limb high_limb, low_limb;
  unsigned sh_1, sh_2;
  mp_size i;
  mp_limb retval;

  if (usize == 0)
    return 0;

  sh_1 = cnt;
  if (sh_1 == 0)
    {
      if (wp != up)
	{
	  /* Copy from high end to low end, to allow specified input/output
	     overlapping.  */
	  for (i = usize - 1; i >= 0; i--)
	    wp[i] = up[i];
	}
      return 0;
    }

  wp += 1;
  sh_2 = BITS_PER_MP_LIMB - sh_1;
  i = usize - 1;
  low_limb = up[i];
  retval = low_limb >> sh_2;
  high_limb = low_limb;
  while (--i >= 0)
    {
      low_limb = up[i];
      wp[i] = (high_limb << sh_1) | (low_limb >> sh_2);
      high_limb = low_limb;
    }
  wp[i] = high_limb << sh_1;

  return retval;
}
