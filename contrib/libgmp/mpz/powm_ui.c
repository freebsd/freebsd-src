/* mpz_powm_ui(res,base,exp,mod) -- Set RES to (base**exp) mod MOD.

Copyright (C) 1991, 1993, 1994, 1996 Free Software Foundation, Inc.

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

void
#if __STDC__
mpz_powm_ui (mpz_ptr res, mpz_srcptr base, unsigned long int exp, mpz_srcptr mod)
#else
mpz_powm_ui (res, base, exp, mod)
     mpz_ptr res;
     mpz_srcptr base;
     unsigned long int exp;
     mpz_srcptr mod;
#endif
{
  mp_ptr rp, mp, bp;
  mp_size_t msize, bsize, rsize;
  mp_size_t size;
  int mod_shift_cnt;
  int negative_result;
  mp_limb_t *free_me = NULL;
  size_t free_me_size;
  TMP_DECL (marker);

  msize = ABS (mod->_mp_size);
  size = 2 * msize;

  rp = res->_mp_d;

  if (msize == 0)
    msize = 1 / msize;		/* provoke a signal */

  if (exp == 0)
    {
      rp[0] = 1;
      res->_mp_size = (msize == 1 && (mod->_mp_d)[0] == 1) ? 0 : 1;
      return;
    }

  TMP_MARK (marker);

  /* Normalize MOD (i.e. make its most significant bit set) as required by
     mpn_divmod.  This will make the intermediate values in the calculation
     slightly larger, but the correct result is obtained after a final
     reduction using the original MOD value.  */

  mp = (mp_ptr) TMP_ALLOC (msize * BYTES_PER_MP_LIMB);
  count_leading_zeros (mod_shift_cnt, mod->_mp_d[msize - 1]);
  if (mod_shift_cnt != 0)
    mpn_lshift (mp, mod->_mp_d, msize, mod_shift_cnt);
  else
    MPN_COPY (mp, mod->_mp_d, msize);

  bsize = ABS (base->_mp_size);
  if (bsize > msize)
    {
      /* The base is larger than the module.  Reduce it.  */

      /* Allocate (BSIZE + 1) with space for remainder and quotient.
	 (The quotient is (bsize - msize + 1) limbs.)  */
      bp = (mp_ptr) TMP_ALLOC ((bsize + 1) * BYTES_PER_MP_LIMB);
      MPN_COPY (bp, base->_mp_d, bsize);
      /* We don't care about the quotient, store it above the remainder,
	 at BP + MSIZE.  */
      mpn_divmod (bp + msize, bp, bsize, mp, msize);
      bsize = msize;
      /* Canonicalize the base, since we are going to multiply with it
	 quite a few times.  */
      MPN_NORMALIZE (bp, bsize);
    }
  else
    bp = base->_mp_d;

  if (bsize == 0)
    {
      res->_mp_size = 0;
      TMP_FREE (marker);
      return;
    }

  if (res->_mp_alloc < size)
    {
      /* We have to allocate more space for RES.  If any of the input
	 parameters are identical to RES, defer deallocation of the old
	 space.  */

      if (rp == mp || rp == bp)
	{
	  free_me = rp;
	  free_me_size = res->_mp_alloc;
	}
      else
	(*_mp_free_func) (rp, res->_mp_alloc * BYTES_PER_MP_LIMB);

      rp = (mp_ptr) (*_mp_allocate_func) (size * BYTES_PER_MP_LIMB);
      res->_mp_alloc = size;
      res->_mp_d = rp;
    }
  else
    {
      /* Make BASE, EXP and MOD not overlap with RES.  */
      if (rp == bp)
	{
	  /* RES and BASE are identical.  Allocate temp. space for BASE.  */
	  bp = (mp_ptr) TMP_ALLOC (bsize * BYTES_PER_MP_LIMB);
	  MPN_COPY (bp, rp, bsize);
	}
      if (rp == mp)
	{
	  /* RES and MOD are identical.  Allocate temporary space for MOD.  */
	  mp = (mp_ptr) TMP_ALLOC (msize * BYTES_PER_MP_LIMB);
	  MPN_COPY (mp, rp, msize);
	}
    }

  MPN_COPY (rp, bp, bsize);
  rsize = bsize;

  {
    mp_ptr xp = (mp_ptr) TMP_ALLOC (2 * (msize + 1) * BYTES_PER_MP_LIMB);
    int c;
    mp_limb_t e;
    mp_limb_t carry_limb;

    negative_result = (exp & 1) && base->_mp_size < 0;

    e = exp;
    count_leading_zeros (c, e);
    e = (e << c) << 1;		/* shift the exp bits to the left, lose msb */
    c = BITS_PER_MP_LIMB - 1 - c;

    /* Main loop.

       Make the result be pointed to alternately by XP and RP.  This
       helps us avoid block copying, which would otherwise be necessary
       with the overlap restrictions of mpn_divmod.  With 50% probability
       the result after this loop will be in the area originally pointed
       by RP (==RES->_mp_d), and with 50% probability in the area originally
       pointed to by XP.  */

    while (c != 0)
      {
	mp_ptr tp;
	mp_size_t xsize;

	mpn_mul_n (xp, rp, rp, rsize);
	xsize = 2 * rsize;
	if (xsize > msize)
	  {
	    mpn_divmod (xp + msize, xp, xsize, mp, msize);
	    xsize = msize;
	  }

	tp = rp; rp = xp; xp = tp;
	rsize = xsize;

	if ((mp_limb_signed_t) e < 0)
	  {
	    mpn_mul (xp, rp, rsize, bp, bsize);
	    xsize = rsize + bsize;
	    if (xsize > msize)
	      {
		mpn_divmod (xp + msize, xp, xsize, mp, msize);
		xsize = msize;
	      }

	    tp = rp; rp = xp; xp = tp;
	    rsize = xsize;
	  }
	e <<= 1;
	c--;
      }

    /* We shifted MOD, the modulo reduction argument, left MOD_SHIFT_CNT
       steps.  Adjust the result by reducing it with the original MOD.

       Also make sure the result is put in RES->_mp_d (where it already
       might be, see above).  */

    if (mod_shift_cnt != 0)
      {
	carry_limb = mpn_lshift (res->_mp_d, rp, rsize, mod_shift_cnt);
	rp = res->_mp_d;
	if (carry_limb != 0)
	  {
	    rp[rsize] = carry_limb;
	    rsize++;
	  }
      }
    else
      {
	MPN_COPY (res->_mp_d, rp, rsize);
	rp = res->_mp_d;
      }

    if (rsize >= msize)
      {
	mpn_divmod (rp + msize, rp, rsize, mp, msize);
	rsize = msize;
      }

    /* Remove any leading zero words from the result.  */
    if (mod_shift_cnt != 0)
      mpn_rshift (rp, rp, rsize, mod_shift_cnt);
    MPN_NORMALIZE (rp, rsize);
  }

  res->_mp_size = negative_result == 0 ? rsize : -rsize;

  if (free_me != NULL)
    (*_mp_free_func) (free_me, free_me_size * BYTES_PER_MP_LIMB);
  TMP_FREE (marker);
}
