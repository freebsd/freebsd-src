/* mpz_add -- Add two integers.

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

#ifndef BERKELEY_MP
void
#ifdef __STDC__
mpz_add (MP_INT *sum, const MP_INT *u, const MP_INT *v)
#else
mpz_add (sum, u, v)
     MP_INT *sum;
     const MP_INT *u;
     const MP_INT *v;
#endif
#else /* BERKELEY_MP */
void
#ifdef __STDC__
madd (const MP_INT *u, const MP_INT *v, MP_INT *sum)
#else
madd (u, v, sum)
     const MP_INT *u;
     const MP_INT *v;
     MP_INT *sum;
#endif
#endif /* BERKELEY_MP */
{
  mp_srcptr up, vp;
  mp_ptr sump;
  mp_size usize, vsize, sumsize;
  mp_size abs_usize;
  mp_size abs_vsize;

  usize = u->size;
  vsize = v->size;
  abs_usize = ABS (usize);
  abs_vsize = ABS (vsize);

  if (abs_usize < abs_vsize)
    {
      /* Swap U and V. */
      {const MP_INT *t = u; u = v; v = t;}
      {mp_size t = usize; usize = vsize; vsize = t;}
      {mp_size t = abs_usize; abs_usize = abs_vsize; abs_vsize = t;}
    }

  /* True: abs(USIZE) >= abs(VSIZE) */

  /* If not space for sum (and possible carry), increase space.  */
  sumsize = abs_usize + 1;
  if (sum->alloc < sumsize)
    _mpz_realloc (sum, sumsize);

  /* These must be after realloc (u or v may be the same as sum).  */
  up = u->d;
  vp = v->d;
  sump = sum->d;

  if (usize >= 0)
    {
      if (vsize >= 0)
	{
	  sumsize = mpn_add (sump, up, abs_usize, vp, abs_vsize);
	  if (sumsize != 0)
	    sump[abs_usize] = 1;
	  sumsize = sumsize + abs_usize;
	}
      else
	{
	  /* The signs are different.  Need exact comparision to determine
	     which operand to subtract from which.  */
	  if (abs_usize == abs_vsize && mpn_cmp (up, vp, abs_usize) < 0)
	    sumsize = -(abs_usize
			+ mpn_sub (sump, vp, abs_usize, up, abs_usize));
	  else
	    sumsize = (abs_usize
		       + mpn_sub (sump, up, abs_usize, vp, abs_vsize));
	}
    }
  else
    {
      if (vsize >= 0)
	{
	  /* The signs are different.  Need exact comparision to determine
	     which operand to subtract from which.  */
	  if (abs_usize == abs_vsize && mpn_cmp (up, vp, abs_usize) < 0)
	    sumsize = (abs_usize
		       + mpn_sub (sump, vp, abs_usize, up, abs_usize));
	  else
	    sumsize = -(abs_usize
			+ mpn_sub (sump, up, abs_usize, vp, abs_vsize));
	}
      else
	{
	  sumsize = mpn_add (sump, up, abs_usize, vp, abs_vsize);
	  if (sumsize != 0)
	    sump[abs_usize] = 1;
	  sumsize = -(sumsize + abs_usize);
	}
    }

  sum->size = sumsize;
}
