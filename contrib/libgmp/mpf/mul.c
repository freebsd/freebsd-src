/* mpf_mul -- Multiply two floats.

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
mpf_mul (mpf_ptr r, mpf_srcptr u, mpf_srcptr v)
#else
mpf_mul (r, u, v)
     mpf_ptr r;
     mpf_srcptr u;
     mpf_srcptr v;
#endif
{
  mp_srcptr up, vp;
  mp_size_t usize, vsize;
  mp_size_t sign_product;
  mp_size_t prec = r->_mp_prec;
  TMP_DECL (marker);

  TMP_MARK (marker);
  usize = u->_mp_size;
  vsize = v->_mp_size;
  sign_product = usize ^ vsize;

  usize = ABS (usize);
  vsize = ABS (vsize);

  up = u->_mp_d;
  vp = v->_mp_d;
  if (usize > prec)
    {
      up += usize - prec;
      usize = prec;
    }
  if (vsize > prec)
    {
      vp += vsize - prec;
      vsize = prec;
    }

  if (usize == 0 || vsize == 0)
    {
      r->_mp_size = 0;
      r->_mp_exp = 0;		/* ??? */
    }
  else
    {
      mp_size_t rsize;
      mp_limb_t cy_limb;
      mp_ptr rp, tp;
      mp_size_t adj;

      rsize = usize + vsize;
      tp = (mp_ptr) TMP_ALLOC (rsize * BYTES_PER_MP_LIMB);
      cy_limb = (usize >= vsize
		 ? mpn_mul (tp, up, usize, vp, vsize)
		 : mpn_mul (tp, vp, vsize, up, usize));

      adj = cy_limb == 0;
      rsize -= adj;
      prec++;
      if (rsize > prec)
	{
	  tp += rsize - prec;
	  rsize = prec;
	}
      rp = r->_mp_d;
      MPN_COPY (rp, tp, rsize);
      r->_mp_exp = u->_mp_exp + v->_mp_exp - adj;
      r->_mp_size = sign_product >= 0 ? rsize : -rsize;
    }
  TMP_FREE (marker);
}
