/* mpf_div -- Divide two floats.

Copyright (C) 1993, 1994, 1996 Free Software Foundation, Inc.

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
mpf_div (mpf_ptr r, mpf_srcptr u, mpf_srcptr v)
#else
mpf_div (r, u, v)
     mpf_ptr r;
     mpf_srcptr u;
     mpf_srcptr v;
#endif
{
  mp_srcptr up, vp;
  mp_ptr rp, tp, rtp;
  mp_size_t usize, vsize;
  mp_size_t rsize, tsize;
  mp_size_t sign_quotient;
  mp_size_t prec;
  unsigned normalization_steps;
  mp_limb_t q_limb;
  mp_exp_t rexp;
  TMP_DECL (marker);

  usize = u->_mp_size;
  vsize = v->_mp_size;
  sign_quotient = usize ^ vsize;
  usize = ABS (usize);
  vsize = ABS (vsize);
  prec = r->_mp_prec;

  if (vsize == 0)
    vsize = 1 / vsize;		/* divide by zero as directed */
  if (usize == 0)
    {
      r->_mp_size = 0;
      r->_mp_exp = 0;
      return;
    }

  TMP_MARK (marker);
  rexp = u->_mp_exp - v->_mp_exp;

  rp = r->_mp_d;
  up = u->_mp_d;
  vp = v->_mp_d;

  if (vsize > prec)
    {
      vp += vsize - prec;
      vsize = prec;
    }

  tsize = vsize + prec;
  tp = (mp_ptr) TMP_ALLOC ((tsize + 1) * BYTES_PER_MP_LIMB);

  if (usize > tsize)
    {
      up += usize - tsize;
      usize = tsize;
      rtp = tp;
    }
  else
    {
      MPN_ZERO (tp, tsize - usize);
      rtp = tp + (tsize - usize);
    }

  count_leading_zeros (normalization_steps, vp[vsize - 1]);

  /* Normalize the divisor and the dividend.  */
  if (normalization_steps != 0)
    {
      mp_ptr tmp;
      mp_limb_t nlimb;

      /* Shift up the divisor setting the most significant bit of
	 the most significant limb.  Use temporary storage not to clobber
	 the original contents of the divisor.  */
      tmp = (mp_ptr) TMP_ALLOC (vsize * BYTES_PER_MP_LIMB);
      mpn_lshift (tmp, vp, vsize, normalization_steps);
      vp = tmp;

      /* Shift up the dividend, possibly introducing a new most
	 significant word.  Move the shifted dividend in the remainder
	 at the same time.  */
      nlimb = mpn_lshift (rtp, up, usize, normalization_steps);
      if (nlimb != 0)
	{
	  rtp[usize] = nlimb;
	  tsize++;
	  rexp++;
	}
    }
  else
    {
      /* The divisor is already normalized, as required.
	 Copy it to temporary space if it overlaps with the quotient.  */
      if (vp - rp <= tsize - vsize)
	{
	  mp_ptr tmp = (mp_ptr) TMP_ALLOC (vsize * BYTES_PER_MP_LIMB);
	  MPN_COPY (tmp, vp, vsize);
	  vp = (mp_srcptr) tmp;
	}

      /* Move the dividend to the remainder.  */
      MPN_COPY (rtp, up, usize);
    }

  q_limb = mpn_divmod (rp, tp, tsize, vp, vsize);
  rsize = tsize - vsize;
  if (q_limb)
    {
      rp[rsize] = q_limb;
      rsize++;
      rexp++;
    }

  r->_mp_size = sign_quotient >= 0 ? rsize : -rsize;
  r->_mp_exp = rexp;
  TMP_FREE (marker);
}
