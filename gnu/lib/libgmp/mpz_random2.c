/* mpz_random2 -- Generate a positive random MP_INT of specified size, with
   long runs of consecutive ones and zeros in the binary representation.
   Meant for testing of other MP routines.

Copyright (C) 1991, 1993 Free Software Foundation, Inc.

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

#if defined (hpux) || defined (__alpha__)
/* HPUX lacks random().  DEC Alpha's random() returns a double.  */
static inline long
random ()
{
  return mrand48 ();
}
#else
long random ();
#endif

void
#ifdef __STDC__
mpz_random2 (MP_INT *x, mp_size size)
#else
mpz_random2 (x, size)
     MP_INT *x;
     mp_size size;
#endif
{
  mp_limb ran, cy_limb;
  mp_ptr xp;
  mp_size xsize, abs_size;
  int n_bits;

  abs_size = ABS (size);

  if (abs_size != 0)
    {
      if (x->alloc < abs_size)
	_mpz_realloc (x, abs_size);
      xp = x->d;

      xp[0] = 1;
      for (xsize = 1;; )
	{
	  ran = random ();
	  n_bits = (ran >> 1) % BITS_PER_MP_LIMB;

	  if (n_bits == 0)
	    {
	      if (xsize == abs_size)
		break;
	    }
	  else
	    {
	      /* Would we get a too large result in mpn_lshift?  */
	      if (xsize == abs_size
		  && (xp[xsize - 1] >> (BITS_PER_MP_LIMB - n_bits)) != 0)
		break;

	      cy_limb = mpn_lshift (xp, xp, xsize, n_bits);
	      if (cy_limb != 0)
		xp[xsize++] = cy_limb;

	      if (ran & 1)
		xp[0] |= ((mp_limb) 1 << n_bits) - 1;
	    }
	}
    }

  x->size = size;
}
