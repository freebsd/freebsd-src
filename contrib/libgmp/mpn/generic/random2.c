/* mpn_random2 -- Generate random numbers with relatively long strings
   of ones and zeroes.  Suitable for border testing.

Copyright (C) 1992, 1993, 1994, 1996 Free Software Foundation, Inc.

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

#if defined (__hpux) || defined (alpha__)  || defined (__svr4__) || defined (__SVR4)
/* HPUX lacks random().  DEC OSF/1 1.2 random() returns a double.  */
long mrand48 ();
static inline long
random ()
{
  return mrand48 ();
}
#else
long random ();
#endif

/* It's a bit tricky to get this right, so please test the code well
   if you hack with it.  Some early versions of the function produced
   random numbers with the leading limb == 0, and some versions never
   made the most significant bit set.  */

void
mpn_random2 (res_ptr, size)
     mp_ptr res_ptr;
     mp_size_t size;
{
  int n_bits;
  int bit_pos;
  mp_size_t limb_pos;
  unsigned int ran;
  mp_limb_t limb;

  limb = 0;

  /* Start off in a random bit position in the most significant limb.  */
  bit_pos = random () & (BITS_PER_MP_LIMB - 1);

  /* Least significant bit of RAN chooses string of ones/string of zeroes.
     Make most significant limb be non-zero by setting bit 0 of RAN.  */
  ran = random () | 1;

  for (limb_pos = size - 1; limb_pos >= 0; )
    {
      n_bits = (ran >> 1) % BITS_PER_MP_LIMB + 1;
      if ((ran & 1) != 0)
	{
	  /* Generate a string of ones.  */
	  if (n_bits >= bit_pos)
	    {
	      res_ptr[limb_pos--] = limb | ((((mp_limb_t) 2) << bit_pos) - 1);
	      bit_pos += BITS_PER_MP_LIMB;
	      limb = (~(mp_limb_t) 0) << (bit_pos - n_bits);
	    }
	  else
	    {
	      limb |= ((((mp_limb_t) 1) << n_bits) - 1) << (bit_pos - n_bits + 1);
	    }
	}
      else
	{
	  /* Generate a string of zeroes.  */
	  if (n_bits >= bit_pos)
	    {
	      res_ptr[limb_pos--] = limb;
	      limb = 0;
	      bit_pos += BITS_PER_MP_LIMB;
	    }
	}
      bit_pos -= n_bits;
      ran = random ();
    }
}
