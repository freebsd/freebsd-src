/* mpf_reldiff -- Generate the relative difference of two floats.

Copyright (C) 1996 Free Software Foundation, Inc.

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
mpf_reldiff (mpf_t rdiff, mpf_srcptr x, mpf_srcptr y)
#else
mpf_reldiff (rdiff, x, y)
     mpf_t rdiff;
     mpf_srcptr x;
     mpf_srcptr y;
#endif
{
  if (mpf_cmp_ui (x, 0) == 0)
    {
      mpf_set_ui (rdiff, (unsigned long int) (mpf_sgn (y) != 0));
    }
  else
    {
      mpf_t d;
      mp_limb_t tmp_limb[2];

      d->_mp_prec = 1;
      d->_mp_d = tmp_limb;

      mpf_sub (d, x, y);
      mpf_abs (d, d);
      mpf_div (rdiff, d, x);
    }
}

