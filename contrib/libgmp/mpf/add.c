/* mpf_add -- Add two floats.

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

void
#if __STDC__
mpf_add (mpf_ptr r, mpf_srcptr u, mpf_srcptr v)
#else
mpf_add (r, u, v)
     mpf_ptr r;
     mpf_srcptr u;
     mpf_srcptr v;
#endif
{
  mp_srcptr up, vp;
  mp_ptr rp, tp;
  mp_size_t usize, vsize, rsize;
  mp_size_t prec;
  mp_exp_t uexp;
  mp_size_t ediff;
  mp_limb_t cy;
  int negate;
  TMP_DECL (marker);

  usize = u->_mp_size;
  vsize = v->_mp_size;

  /* Handle special cases that don't work in generic code below.  */
  if (usize == 0)
    {
      mpf_set (r, v);
      return;
    }
  if (vsize == 0)
    {
      mpf_set (r, u);
      return;
    }

  /* If signs of U and V are different, perform subtraction.  */
  if ((usize ^ vsize) < 0)
    {
      __mpf_struct v_negated;
      v_negated._mp_size = -vsize;
      v_negated._mp_exp = v->_mp_exp;
      v_negated._mp_d = v->_mp_d;
      mpf_sub (r, u, &v_negated);
      return;
    }

  TMP_MARK (marker);

  /* Signs are now known to be the same.  */
  negate = usize < 0;

  /* Make U be the operand with the largest exponent.  */
  if (u->_mp_exp < v->_mp_exp)
    {
      mpf_srcptr t;
      t = u; u = v; v = t;
      usize = u->_mp_size;
      vsize = v->_mp_size;
    }

  usize = ABS (usize);
  vsize = ABS (vsize);
  up = u->_mp_d;
  vp = v->_mp_d;
  rp = r->_mp_d;
  prec = r->_mp_prec;
  uexp = u->_mp_exp;
  ediff = u->_mp_exp - v->_mp_exp;

  /* If U extends beyond PREC, ignore the part that does.  */
  if (usize > prec)
    {
      up += usize - prec;
      usize = prec;
    }

  /* If V extends beyond PREC, ignore the part that does.
     Note that this may make vsize negative.  */
  if (vsize + ediff > prec)
    {
      vp += vsize + ediff - prec;
      vsize = prec - ediff;
    }

#if 0
  /* Locate the least significant non-zero limb in (the needed parts
     of) U and V, to simplify the code below.  */
  while (up[0] == 0)
    up++, usize--;
  while (vp[0] == 0)
    vp++, vsize--;
#endif

  /* Allocate temp space for the result.  Allocate
     just vsize + ediff later???  */
  tp = (mp_ptr) TMP_ALLOC (prec * BYTES_PER_MP_LIMB);

  if (ediff >= prec)
    {
      /* V completely cancelled.  */
      if (tp != up)
	MPN_COPY (rp, up, usize);
      rsize = usize;
    }
  else
    {
      /* uuuu     |  uuuu     |  uuuu     |  uuuu     |  uuuu    */
      /* vvvvvvv  |  vv       |    vvvvv  |    v      |       vv */

      if (usize > ediff)
	{
	  /* U and V partially overlaps.  */
	  if (vsize + ediff <= usize)
	    {
	      /* uuuu     */
	      /*   v      */
	      mp_size_t size;
	      size = usize - ediff - vsize;
	      MPN_COPY (tp, up, size);
	      cy = mpn_add (tp + size, up + size, usize - size, vp, vsize);
	      rsize = usize;
	    }
	  else
	    {
	      /* uuuu     */
	      /*   vvvvv  */
	      mp_size_t size;
	      size = vsize + ediff - usize;
	      MPN_COPY (tp, vp, size);
	      cy = mpn_add (tp + size, up, usize, vp + size, usize - ediff);
	      rsize = vsize + ediff;
	    }
	}
      else
	{
	  /* uuuu     */
	  /*      vv  */
	  mp_size_t size;
	  size = vsize + ediff - usize;
	  MPN_COPY (tp, vp, vsize);
	  MPN_ZERO (tp + vsize, ediff - usize);
	  MPN_COPY (tp + size, up, usize);
	  cy = 0;
	  rsize = size + usize;
	}

      MPN_COPY (rp, tp, rsize);
      rp[rsize] = cy;
      rsize += cy;
      uexp += cy;
    }

  r->_mp_size = negate ? -rsize : rsize;
  r->_mp_exp = uexp;
  TMP_FREE (marker);
}
