/* mpn_scan0 -- Scan from a given bit position for the next clear bit.

Copyright (C) 1994, 1996 Free Software Foundation, Inc.

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

/* Design issues:
   1. What if starting_bit is not within U?  Caller's problem?
   2. Bit index should be 'unsigned'?

   Argument constraints:
   1. U must sooner ot later have a limb with a clear bit.
 */

unsigned long int
#if __STDC__
mpn_scan0 (register mp_srcptr up,
	   register unsigned long int starting_bit)
#else
mpn_scan0 (up, starting_bit)
     register mp_srcptr up;
     register unsigned long int starting_bit;
#endif
{
  mp_size_t starting_word;
  mp_limb_t alimb;
  int cnt;
  mp_srcptr p;

  /* Start at the word implied by STARTING_BIT.  */
  starting_word = starting_bit / BITS_PER_MP_LIMB;
  p = up + starting_word;
  alimb = ~*p++;

  /* Mask off any bits before STARTING_BIT in the first limb.  */
  alimb &= - (mp_limb_t) 1 << (starting_bit % BITS_PER_MP_LIMB);

  while (alimb == 0)
    alimb = ~*p++;

  count_leading_zeros (cnt, alimb & -alimb);
  return (p - up) * BITS_PER_MP_LIMB - 1 - cnt;
}
