/* mpz_powm_ui(res,base,exp,mod) -- Set RES to (base**exp) mod MOD.

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

void
#ifdef __STDC__
mpz_powm_ui (MP_INT *res, const MP_INT *base, unsigned long int exp,
	     const MP_INT *mod)
#else
mpz_powm_ui (res, base, exp, mod)
     MP_INT *res;
     const MP_INT *base;
     unsigned long int exp;
     const MP_INT *mod;
#endif
{
  mp_ptr rp, mp, bp;
  mp_size msize, bsize, rsize;
  mp_size size;
  int mod_shift_cnt;
  int negative_result;
  mp_limb *free_me = NULL;
  size_t free_me_size;

  msize = ABS (mod->size);
  size = 2 * msize;

  rp = res->d;

  /* Normalize MOD (i.e. make its most significant bit set) as required by
     mpn_div.  This will make the intermediate values in the calculation
     slightly larger, but the correct result is obtained after a final
     reduction using the original MOD value.  */

  mp = (mp_ptr) alloca (msize * BYTES_PER_MP_LIMB);
  count_leading_zeros (mod_shift_cnt, mod->d[msize - 1]);
  if (mod_shift_cnt != 0)
    (void) mpn_lshift (mp, mod->d, msize, mod_shift_cnt);
  else
    MPN_COPY (mp, mod->d, msize);

  bsize = ABS (base->size);
  if (bsize > msize)
    {
      /* The base is larger than the module.  Reduce it.  */

      /* Allocate (BSIZE + 1) with space for remainder and quotient.
	 (The quotient is (bsize - msize + 1) limbs.)  */
      bp = (mp_ptr) alloca ((bsize + 1) * BYTES_PER_MP_LIMB);
      MPN_COPY (bp, base->d, bsize);
      /* We don't care about the quotient, store it above the remainder,
	 at BP + MSIZE.  */
      mpn_div (bp + msize, bp, bsize, mp, msize);
      bsize = msize;
      while (bsize > 0 && bp[bsize - 1] == 0)
	bsize--;
    }
  else
    {
      bp = base->d;
      bsize = ABS (base->size);
    }

  if (res->alloc < size)
    {
      /* We have to allocate more space for RES.  If any of the input
	 parameters are identical to RES, defer deallocation of the old
	 space.  */

      if (rp == mp || rp == bp)
	{
	  free_me = rp;
	  free_me_size = res->alloc;
	}
      else
	(*_mp_free_func) (rp, res->alloc * BYTES_PER_MP_LIMB);

      rp = (mp_ptr) (*_mp_allocate_func) (size * BYTES_PER_MP_LIMB);
      res->alloc = size;
      res->d = rp;
    }
  else
    {
      /* Make BASE, EXP and MOD not overlap with RES.  */
      if (rp == bp)
	{
	  /* RES and BASE are identical.  Allocate temp. space for BASE.  */
	  bp = (mp_ptr) alloca (bsize * BYTES_PER_MP_LIMB);
	  MPN_COPY (bp, rp, bsize);
	}
      if (rp == mp)
	{
	  /* RES and MOD are identical.  Allocate temporary space for MOD.  */
	  mp = (mp_ptr) alloca (msize * BYTES_PER_MP_LIMB);
	  MPN_COPY (mp, rp, msize);
	}
    }

  if (exp == 0)
    {
      rp[0] = 1;
      res->size = 1;
      return;
    }

  MPN_COPY (rp, bp, bsize);
  rsize = bsize;

  {
    mp_size xsize;
    mp_ptr dummyp = (mp_ptr) alloca ((msize + 1) * BYTES_PER_MP_LIMB);
    mp_ptr xp = (mp_ptr) alloca (2 * (msize + 1) * BYTES_PER_MP_LIMB);
    int c;
    mp_limb e;
    mp_limb carry_limb;

    negative_result = (exp & 1) && base->size < 0;

    e = exp;
    count_leading_zeros (c, e);
    e <<= (c + 1);		/* shift the exp bits to the left, loose msb */
    c = BITS_PER_MP_LIMB - 1 - c;

    /* Main loop.

       Make the result be pointed to alternately by XP and RP.  This
       helps us avoid block copying, which would otherwise be necessary
       with the overlap restrictions of mpn_div.  With 50% probability
       the result after this loop will be in the area originally pointed
       by RP (==RES->D), and with 50% probability in the area originally
       pointed to by XP.  */

    while (c != 0)
      {
	mp_ptr tp;
	mp_size tsize;

	xsize = mpn_mul (xp, rp, rsize, rp, rsize);
	mpn_div (dummyp, xp, xsize, mp, msize);

	/* Remove any leading zero words from the result.  */
	if (xsize > msize)
	  xsize = msize;
	while (xsize > 0 && xp[xsize - 1] == 0)
	  xsize--;

	tp = rp; rp = xp; xp = tp;
	tsize = rsize; rsize = xsize; xsize = tsize;

	if ((mp_limb_signed) e < 0)
	  {
	    if (rsize > bsize)
	      xsize = mpn_mul (xp, rp, rsize, bp, bsize);
	    else
	      xsize = mpn_mul (xp, bp, bsize, rp, rsize);
	    mpn_div (dummyp, xp, xsize, mp, msize);

	    /* Remove any leading zero words from the result.  */
	    if (xsize > msize)
	      xsize = msize;
	    while (xsize > 0 && xp[xsize - 1] == 0)
	      xsize--;

	    tp = rp; rp = xp; xp = tp;
	    tsize = rsize; rsize = xsize; xsize = tsize;
	  }
	e <<= 1;
	c--;
      }

    /* We shifted MOD, the modulo reduction argument, left MOD_SHIFT_CNT
       steps.  Adjust the result by reducing it with the original MOD.

       Also make sure the result is put in RES->D (where it already
       might be, see above).  */

    carry_limb = mpn_lshift (res->d, rp, rsize, mod_shift_cnt);
    rp = res->d;
    if (carry_limb != 0)
      {
	rp[rsize] = carry_limb;
	rsize++;
      }
    mpn_div (dummyp, rp, rsize, mp, msize);
    /* Remove any leading zero words from the result.  */
    if (rsize > msize)
      rsize = msize;
    while (rsize > 0 && rp[rsize - 1] == 0)
      rsize--;
    rsize = mpn_rshift (rp, rp, rsize, mod_shift_cnt);
  }

  res->size = negative_result >= 0 ?  rsize : -rsize;

  if (free_me != NULL)
    (*_mp_free_func) (free_me, free_me_size * BYTES_PER_MP_LIMB);

  alloca (0);
}
