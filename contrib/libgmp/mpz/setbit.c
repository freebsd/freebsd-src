/* mpz_setbit -- set a specified bit.

Copyright (C) 1991, 1993, 1994, 1995 Free Software Foundation, Inc.

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

void
#if __STDC__
mpz_setbit (mpz_ptr d, unsigned long int bit_index)
#else
mpz_setbit (d, bit_index)
     mpz_ptr d;
     unsigned long int bit_index;
#endif
{
  mp_size_t dsize = d->_mp_size;
  mp_ptr dp = d->_mp_d;
  mp_size_t limb_index;

  limb_index = bit_index / BITS_PER_MP_LIMB;
  if (dsize >= 0)
    {
      if (limb_index < dsize)
	{
	  dp[limb_index] |= (mp_limb_t) 1 << (bit_index % BITS_PER_MP_LIMB);
	  d->_mp_size = dsize;
	}
      else
	{
	  /* Ugh.  The bit should be set outside of the end of the
	     number.  We have to increase the size of the number.  */
	  if (d->_mp_alloc < limb_index + 1)
	    {
	      _mpz_realloc (d, limb_index + 1);
	      dp = d->_mp_d;
	    }
	  MPN_ZERO (dp + dsize, limb_index - dsize);
	  dp[limb_index] = (mp_limb_t) 1 << (bit_index % BITS_PER_MP_LIMB);
	  d->_mp_size = limb_index + 1;
	}
    }
  else
    {
      mp_size_t zero_bound;

      /* Simulate two's complement arithmetic, i.e. simulate
	 1. Set OP = ~(OP - 1) [with infinitely many leading ones].
	 2. set the bit.
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
	    dp[limb_index] &= ~((mp_limb_t) 1 << (bit_index % BITS_PER_MP_LIMB));
	  else
	    ;
	}
      else if (limb_index == zero_bound)
	{
	  dp[limb_index] = ((dp[limb_index] - 1)
			    & ~((mp_limb_t) 1 << (bit_index % BITS_PER_MP_LIMB))) + 1;
	  if (dp[limb_index] == 0)
	    {
	      mp_size_t i;
	      for (i = limb_index + 1; i < dsize; i++)
		{
		  dp[i] += 1;
		  if (dp[i] != 0)
		    goto fin;
		}
	      /* We got carry all way out beyond the end of D.  Increase
		 its size (and allocation if necessary).  */
	      dsize++;
	      if (d->_mp_alloc < dsize)
		{
		  _mpz_realloc (d, dsize);
		  dp = d->_mp_d;
		}
	      dp[i] = 1;
	      d->_mp_size = -dsize;
	    fin:;
	    }
	}
      else
	;
    }
}
