/* mpz_sub -- Subtract two integers.

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

#ifndef BERKELEY_MP
void
#if __STDC__
mpz_sub (mpz_ptr w, mpz_srcptr u, mpz_srcptr v)
#else
mpz_sub (w, u, v)
     mpz_ptr w;
     mpz_srcptr u;
     mpz_srcptr v;
#endif
#else /* BERKELEY_MP */
void
#if __STDC__
msub (mpz_srcptr u, mpz_srcptr v, mpz_ptr w)
#else
msub (u, v, w)
     mpz_srcptr u;
     mpz_srcptr v;
     mpz_ptr w;
#endif
#endif /* BERKELEY_MP */
{
  mp_srcptr up, vp;
  mp_ptr wp;
  mp_size_t usize, vsize, wsize;
  mp_size_t abs_usize;
  mp_size_t abs_vsize;

  usize = u->_mp_size;
  vsize = -v->_mp_size;		/* The "-" makes the difference from mpz_add */
  abs_usize = ABS (usize);
  abs_vsize = ABS (vsize);

  if (abs_usize < abs_vsize)
    {
      /* Swap U and V. */
      {const __mpz_struct *t = u; u = v; v = t;}
      {mp_size_t t = usize; usize = vsize; vsize = t;}
      {mp_size_t t = abs_usize; abs_usize = abs_vsize; abs_vsize = t;}
    }

  /* True: ABS_USIZE >= ABS_VSIZE.  */

  /* If not space for w (and possible carry), increase space.  */
  wsize = abs_usize + 1;
  if (w->_mp_alloc < wsize)
    _mpz_realloc (w, wsize);

  /* These must be after realloc (u or v may be the same as w).  */
  up = u->_mp_d;
  vp = v->_mp_d;
  wp = w->_mp_d;

  if ((usize ^ vsize) < 0)
    {
      /* U and V have different sign.  Need to compare them to determine
	 which operand to subtract from which.  */

      /* This test is right since ABS_USIZE >= ABS_VSIZE.  */
      if (abs_usize != abs_vsize)
	{
	  mpn_sub (wp, up, abs_usize, vp, abs_vsize);
	  wsize = abs_usize;
	  MPN_NORMALIZE (wp, wsize);
	  if (usize < 0)
	    wsize = -wsize;
	}
      else if (mpn_cmp (up, vp, abs_usize) < 0)
	{
	  mpn_sub_n (wp, vp, up, abs_usize);
	  wsize = abs_usize;
	  MPN_NORMALIZE (wp, wsize);
	  if (usize >= 0)
	    wsize = -wsize;
	}
      else
	{
	  mpn_sub_n (wp, up, vp, abs_usize);
	  wsize = abs_usize;
	  MPN_NORMALIZE (wp, wsize);
	  if (usize < 0)
	    wsize = -wsize;
	}
    }
  else
    {
      /* U and V have same sign.  Add them.  */
      mp_limb_t cy_limb = mpn_add (wp, up, abs_usize, vp, abs_vsize);
      wp[abs_usize] = cy_limb;
      wsize = abs_usize + cy_limb;
      if (usize < 0)
	wsize = -wsize;
    }

  w->_mp_size = wsize;
}
