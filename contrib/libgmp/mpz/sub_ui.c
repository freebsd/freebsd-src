/* mpz_sub_ui -- Subtract an unsigned one-word integer from an MP_INT.

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

void
#if __STDC__
mpz_sub_ui (mpz_ptr w, mpz_srcptr u, unsigned long int v)
#else
mpz_sub_ui (w, u, v)
     mpz_ptr w;
     mpz_srcptr u;
     unsigned long int v;
#endif
{
  mp_srcptr up;
  mp_ptr wp;
  mp_size_t usize, wsize;
  mp_size_t abs_usize;

  usize = u->_mp_size;
  abs_usize = ABS (usize);

  /* If not space for W (and possible carry), increase space.  */
  wsize = abs_usize + 1;
  if (w->_mp_alloc < wsize)
    _mpz_realloc (w, wsize);

  /* These must be after realloc (U may be the same as W).  */
  up = u->_mp_d;
  wp = w->_mp_d;

  if (abs_usize == 0)
    {
      wp[0] = v;
      w->_mp_size = -(v != 0);
      return;
    }

  if (usize < 0)
    {
      mp_limb_t cy;
      cy = mpn_add_1 (wp, up, abs_usize, v);
      wp[abs_usize] = cy;
      wsize = -(abs_usize + cy);
    }
  else
    {
      /* The signs are different.  Need exact comparison to determine
	 which operand to subtract from which.  */
      if (abs_usize == 1 && up[0] < v)
	{
	  wp[0] = v - up[0];
	  wsize = -1;
	}
      else
	{
	  mpn_sub_1 (wp, up, abs_usize, v);
	  /* Size can decrease with at most one limb.  */
	  wsize = abs_usize - (wp[abs_usize - 1] == 0);
	}
    }

  w->_mp_size = wsize;
}
