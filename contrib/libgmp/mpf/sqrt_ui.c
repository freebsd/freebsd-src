/* mpf_sqrt_ui -- Compute the square root of an unsigned integer.

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
mpf_sqrt_ui (mpf_ptr r, unsigned long int u)
#else
mpf_sqrt_ui (r, u)
     mpf_ptr r;
     unsigned long int u;
#endif
{
  mp_size_t rsize;
  mp_ptr tp;
  mp_size_t prec;
  TMP_DECL (marker);

  if (u == 0)
    {
      r->_mp_size = 0;
      r->_mp_exp = 0;
      return;
    }

  TMP_MARK (marker);

  prec = r->_mp_prec;
  rsize = 2 * prec + 1;

  tp = (mp_ptr) TMP_ALLOC (rsize * BYTES_PER_MP_LIMB);

  MPN_ZERO (tp, rsize - 1);
  tp[rsize - 1] = u;

  mpn_sqrtrem (r->_mp_d, NULL, tp, rsize);

  r->_mp_size = prec + 1;
  r->_mp_exp = 1;
  TMP_FREE (marker);
}
