/* mpf_mul_ui -- Multiply a float and an unsigned integer.

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
mpf_mul_ui (mpf_ptr r, mpf_srcptr u, unsigned long int v)
#else
mpf_mul_ui (r, u, v)
     mpf_ptr r;
     mpf_srcptr u;
     unsigned long int v;
#endif
{
  mp_srcptr up;
  mp_size_t usize;
  mp_size_t size;
  mp_size_t prec = r->_mp_prec;
  mp_limb_t cy_limb;
  mp_ptr rp;

  usize = u->_mp_size;
  size = ABS (usize);

  rp = r->_mp_d;
  up = u->_mp_d;
  if (size > prec)
    {
      up += size - prec;
      size = prec;
    }

#if 0
  /* Since we can do it at almost no cost, remove zero limbs at low end of
     result. */
  if (up[0] == 0)
    up++, size--;
#endif

  if (size == 0 || v == 0)
    {
      r->_mp_size = 0;
      r->_mp_exp = 0;		/* ??? */
    }
  else
    {
      cy_limb = mpn_mul_1 (rp, up, size, (mp_limb_t) v);
      rp[size] = cy_limb;
      cy_limb = cy_limb != 0;
      r->_mp_exp = u->_mp_exp + cy_limb;
      size += cy_limb;
      r->_mp_size = usize >= 0 ? size : -size;
    }
}
