/* mpz_div_2exp -- Divide a bignum by 2**CNT

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
mpz_div_2exp (MP_INT *w, const MP_INT *u, unsigned long int cnt)
#else
mpz_div_2exp (w, u, cnt)
     MP_INT *w;
     const MP_INT *u;
     unsigned long int cnt;
#endif
{
  mp_size usize = u->size;
  mp_size wsize;
  mp_size abs_usize = ABS (usize);
  mp_size limb_cnt;

  limb_cnt = cnt / BITS_PER_MP_LIMB;
  wsize = abs_usize - limb_cnt;
  if (wsize <= 0)
    wsize = 0;
  else
    {
      if (w->alloc < wsize)
	_mpz_realloc (w, wsize);

      wsize = mpn_rshift (w->d, u->d + limb_cnt, abs_usize - limb_cnt,
			   cnt % BITS_PER_MP_LIMB);
    }

  w->size = (usize >= 0) ? wsize : -wsize;
}
