/* mpf_sqrt -- Compute the square root of a float.

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
mpf_sqrt (mpf_ptr r, mpf_srcptr u)
#else
mpf_sqrt (r, u)
     mpf_ptr r;
     mpf_srcptr u;
#endif
{
  mp_size_t usize;
  mp_ptr up, tp;
  mp_size_t prec;
  mp_exp_t tsize, rexp;
  TMP_DECL (marker);

  usize = u->_mp_size;
  if (usize <= 0)
    {
      usize = 1 - 1 / (usize == 0);	/* Divide by zero for negative OP.  */
      r->_mp_size = usize;	/* cheat flow by using usize here */
      r->_mp_exp = 0;
      return;
    }

  TMP_MARK (marker);

  prec = r->_mp_prec;
  rexp = (u->_mp_exp + 1) >> 1;	/* round towards -inf */
  tsize = 2 * prec + (u->_mp_exp & 1);

  up = u->_mp_d;
  tp = (mp_ptr) TMP_ALLOC (tsize * BYTES_PER_MP_LIMB);

  if (usize > tsize)
    {
      up += usize - tsize;
      usize = tsize;
      MPN_COPY (tp, up, tsize);
    }
  else
    {
      MPN_ZERO (tp, tsize - usize);
      MPN_COPY (tp + (tsize - usize), up, usize);
    }

  mpn_sqrtrem (r->_mp_d, NULL, tp, tsize);

  r->_mp_size = (tsize + 1) / 2;
  r->_mp_exp = rexp;
  TMP_FREE (marker);
}
