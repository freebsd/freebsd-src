/* mpz_mul_2exp -- Multiply a bignum by 2**CNT

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

void
#ifdef __STDC__
mpz_mul_2exp (MP_INT *w, const MP_INT *u, unsigned long int cnt)
#else
mpz_mul_2exp (w, u, cnt)
     MP_INT *w;
     const MP_INT *u;
     unsigned long int cnt;
#endif
{
  mp_size usize = u->size;
  mp_size abs_usize = ABS (usize);
  mp_size wsize;
  mp_size limb_cnt;
  mp_ptr wp;
  mp_limb wdigit;

  if (usize == 0)
    {
      w->size = 0;
      return;
    }

  limb_cnt = cnt / BITS_PER_MP_LIMB;
  wsize = abs_usize + limb_cnt + 1;
  if (w->alloc < wsize)
    _mpz_realloc (w, wsize);
  wp = w->d;

  wdigit = mpn_lshift (wp + limb_cnt, u->d, abs_usize,
			cnt % BITS_PER_MP_LIMB);
  wsize = abs_usize + limb_cnt;

  if (wdigit != 0)
    {
      wp[wsize] = wdigit;
      wsize++;
    }

  /* Zero all whole digits at low end.  Do it here and not before calling
     mpn_lshift, not to loose for U == W.  */
  MPN_ZERO (wp, limb_cnt);

  w->size = (usize >= 0) ? wsize : -wsize;
}
