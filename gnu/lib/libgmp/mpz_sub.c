/* mpz_sub -- Subtract two integers.

Copyright (C) 1991 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

The GNU MP Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the GNU MP Library; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "gmp.h"
#include "gmp-impl.h"

#ifndef BERKELEY_MP
void
#ifdef __STDC__
mpz_sub (MP_INT *w, const MP_INT *u, const MP_INT *v)
#else
mpz_sub (w, u, v)
     MP_INT *w;
     const MP_INT *u;
     const MP_INT *v;
#endif
#else /* BERKELEY_MP */
void
#ifdef __STDC__
msub (const MP_INT *u, const MP_INT *v, MP_INT *w)
#else
msub (u, v, w)
     const MP_INT *u;
     const MP_INT *v;
     MP_INT *w;
#endif
#endif /* BERKELEY_MP */
{
  mp_srcptr up, vp;
  mp_ptr wp;
  mp_size usize, vsize, wsize;
  mp_size abs_usize;
  mp_size abs_vsize;

  usize = u->size;
  vsize = -v->size;		/* The "-" makes the difference from mpz_add */
  abs_usize = ABS (usize);
  abs_vsize = ABS (vsize);

  if (abs_usize < abs_vsize)
    {
      /* Swap U and V. */
      {const MP_INT *t = u; u = v; v = t;}
      {mp_size t = usize; usize = vsize; vsize = t;}
      {mp_size t = abs_usize; abs_usize = abs_vsize; abs_vsize = t;}
    }

  /* True: abs(USIZE) >= abs(VSIZE) */

  /* If not space for sum (and possible carry), increase space.  */
  wsize = abs_usize + 1;
  if (w->alloc < wsize)
    _mpz_realloc (w, wsize);

  /* These must be after realloc (u or v may be the same as w).  */
  up = u->d;
  vp = v->d;
  wp = w->d;

  if (usize >= 0)
    {
      if (vsize >= 0)
	{
	  wsize = mpn_add (wp, up, abs_usize, vp, abs_vsize);
	  if (wsize != 0)
	    wp[abs_usize] = 1;
	  wsize = wsize + abs_usize;
	}
      else
	{
	  /* The signs are different.  Need exact comparision to determine
	     which operand to subtract from which.  */
	  if (abs_usize == abs_vsize && mpn_cmp (up, vp, abs_usize) < 0)
	    wsize = -(abs_usize + mpn_sub (wp, vp, abs_usize, up, abs_usize));
	  else
	    wsize = abs_usize + mpn_sub (wp, up, abs_usize, vp, abs_vsize);
	}
    }
  else
    {
      if (vsize >= 0)
	{
	  /* The signs are different.  Need exact comparision to determine
	     which operand to subtract from which.  */
	  if (abs_usize == abs_vsize && mpn_cmp (up, vp, abs_usize) < 0)
	    wsize = abs_usize + mpn_sub (wp, vp, abs_usize, up, abs_usize);
	  else
	    wsize = -(abs_usize + mpn_sub (wp, up, abs_usize, vp, abs_vsize));
	}
      else
	{
	  wsize = mpn_add (wp, up, abs_usize, vp, abs_vsize);
	  if (wsize != 0)
	    wp[abs_usize] = 1;
	  wsize = -(wsize + abs_usize);
	}
    }

  w->size = wsize;
}
