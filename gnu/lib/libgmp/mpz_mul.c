/* mpz_mul -- Multiply two integers.

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
mpz_mul (MP_INT *w, const MP_INT *u, const MP_INT *v)
#else
mpz_mul (w, u, v)
     MP_INT *w;
     const MP_INT *u;
     const MP_INT *v;
#endif
#else /* BERKELEY_MP */
void
#ifdef __STDC__
mult (const MP_INT *u, const MP_INT *v, MP_INT *w)
#else
mult (u, v, w)
     const MP_INT *u;
     const MP_INT *v;
     MP_INT *w;
#endif
#endif /* BERKELEY_MP */
{
  mp_size usize = u->size;
  mp_size vsize = v->size;
  mp_size wsize;
  mp_size sign_product;
  mp_ptr up, vp;
  mp_ptr wp;
  mp_ptr free_me = NULL;
  size_t free_me_size;

  sign_product = usize ^ vsize;
  usize = ABS (usize);
  vsize = ABS (vsize);

  if (usize < vsize)
    {
      /* Swap U and V.  */
      {const MP_INT *t = u; u = v; v = t;}
      {mp_size t = usize; usize = vsize; vsize = t;}
    }

  up = u->d;
  vp = v->d;
  wp = w->d;

  /* Ensure W has space enough to store the result.  */
  wsize = usize + vsize;
  if (w->alloc < wsize)
    {
      if (wp == up || wp == vp)
	{
	  free_me = wp;
	  free_me_size = w->alloc;
	}
      else
	(*_mp_free_func) (wp, w->alloc * BYTES_PER_MP_LIMB);

      w->alloc = wsize;
      wp = (mp_ptr) (*_mp_allocate_func) (wsize * BYTES_PER_MP_LIMB);
      w->d = wp;
    }
  else
    {
      /* Make U and V not overlap with W.  */
      if (wp == up)
	{
	  /* W and U are identical.  Allocate temporary space for U.  */
	  up = (mp_ptr) alloca (usize * BYTES_PER_MP_LIMB);
	  /* Is V identical too?  Keep it identical with U.  */
	  if (wp == vp)
	    vp = up;
	  /* Copy to the temporary space.  */
	  MPN_COPY (up, wp, usize);
	}
      else if (wp == vp)
	{
	  /* W and V are identical.  Allocate temporary space for V.  */
	  vp = (mp_ptr) alloca (vsize * BYTES_PER_MP_LIMB);
	  /* Copy to the temporary space.  */
	  MPN_COPY (vp, wp, vsize);
	}
    }

  wsize = mpn_mul (wp, up, usize, vp, vsize);
  w->size = sign_product < 0 ? -wsize : wsize;
  if (free_me != NULL)
    (*_mp_free_func) (free_me, free_me_size * BYTES_PER_MP_LIMB);

  alloca (0);
}
