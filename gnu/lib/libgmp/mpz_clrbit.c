/* mpz_clrbit -- clear a specified bit.

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

#define MPN_NORMALIZE(p, size) \
  do {									\
    mp_size i;								\
    for (i = (size) - 1; i >= 0; i--)					\
      if ((p)[i] != 0)							\
	break;								\
    (size) = i + 1;							\
  } while (0)

void
#ifdef __STDC__
mpz_clrbit (MP_INT *d, unsigned long int bit_index)
#else
mpz_clrbit (d, bit_index)
     MP_INT *d;
     unsigned long int bit_index;
#endif
{
  mp_size dsize = d->size;
  mp_ptr dp = d->d;
  mp_size limb_index;

  limb_index = bit_index / BITS_PER_MP_LIMB;
  if (dsize >= 0)
    {
      if (limb_index < dsize)
	{
	  dp[limb_index] &= ~((mp_limb) 1 << (bit_index % BITS_PER_MP_LIMB));
	  MPN_NORMALIZE (dp, dsize);
	  d->size = dsize;
	}
      else
	;
    }
  else
    {
      mp_size zero_bound;

      /* Simulate two's complement arithmetic, i.e. simulate
	 1. Set OP = ~(OP - 1) [with infinitely many leading ones].
	 2. clear the bit.
	 3. Set OP = ~OP + 1.  */

      dsize = -dsize;

      /* No upper bound on this loop, we're sure there's a non-zero limb
	 sooner ot later.  */
      for (zero_bound = 0; ; zero_bound++)
	if (dp[zero_bound] != 0)
	  break;

      if (limb_index > zero_bound)
	{
	  if (limb_index < dsize)
	    {
	      dp[limb_index] |= ((mp_limb) 1 << (bit_index % BITS_PER_MP_LIMB));
	    }
	  else
	    {
	      /* Ugh.  The bit should be cleared outside of the end of the
		 number.  We have to increase the size of the number.  */
	      if (d->alloc < limb_index + 1)
		{
		  _mpz_realloc (d, limb_index + 1);
		  dp = d->d;
		}
	      MPN_ZERO (dp + dsize, limb_index - dsize);
	      dp[limb_index] = ((mp_limb) 1 << (bit_index % BITS_PER_MP_LIMB));
	      d->size = -(limb_index + 1);
	    }
	}
      else if (limb_index == zero_bound)
	{
	  dp[limb_index] = ((dp[limb_index] - 1)
			    | ((mp_limb) 1 << (bit_index % BITS_PER_MP_LIMB))) + 1;
	  if (dp[limb_index] == 0)
	    {
	      mp_size i;
	      for (i = limb_index + 1; i < dsize; i++)
		{
		  dp[i] += 1;
		  if (dp[i] != 0)
		    goto fin;
		}
	      /* We got carry all way out beyond the end of D.  Increase
		 its size (and allocation if necessary).  */
	      dsize++;
	      if (d->alloc < dsize)
		{
		  _mpz_realloc (d, dsize);
		  dp = d->d;
		}
	      dp[i] = 1;
	      d->size = -dsize;
	    fin:;
	    }
	}
      else
	;
    }
}
