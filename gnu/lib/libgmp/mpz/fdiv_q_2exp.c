/* mpz_fdiv_q_2exp -- Divide an integer by 2**CNT.  Round the quotient
   towards -infinity.

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
mpz_fdiv_q_2exp (mpz_ptr w, mpz_srcptr u, unsigned long int cnt)
#else
mpz_fdiv_q_2exp (w, u, cnt)
     mpz_ptr w;
     mpz_srcptr u;
     unsigned long int cnt;
#endif
{
  mp_size_t usize = u->_mp_size;
  mp_size_t wsize;
  mp_size_t abs_usize = ABS (usize);
  mp_size_t limb_cnt;
  mp_ptr wp;
  mp_limb_t round = 0;

  limb_cnt = cnt / BITS_PER_MP_LIMB;
  wsize = abs_usize - limb_cnt;
  if (wsize <= 0)
    {
      wp = w->_mp_d;
      wsize = 0;
      /* Set ROUND since we know we skip some non-zero words in this case.
	 Well, if U is zero, we don't, but then this will be taken care of
	 below, since rounding only really takes place for negative U.  */
      round = 1;
      wp[0] = 1;
      w->_mp_size = -(usize < 0);
      return;
    }
  else
    {
      mp_size_t i;
      mp_ptr up;

      /* Make sure there is enough space.  We make an extra limb
	 here to account for possible rounding at the end.  */
      if (w->_mp_alloc < wsize + 1)
	_mpz_realloc (w, wsize + 1);

      wp = w->_mp_d;
      up = u->_mp_d;

      /* Set ROUND if we are about skip some non-zero limbs.  */
      for (i = 0; i < limb_cnt && round == 0; i++)
	round = up[i];

      cnt %= BITS_PER_MP_LIMB;
      if (cnt != 0)
	{
	  round |= mpn_rshift (wp, up + limb_cnt, wsize, cnt);
	  wsize -= wp[wsize - 1] == 0;
	}
      else
	{
	  MPN_COPY_INCR (wp, up + limb_cnt, wsize);
	}
    }

  if (usize < 0 && round != 0)
    {
      mp_limb_t cy;
      cy = mpn_add_1 (wp, wp, wsize, 1);
      wp[wsize] = cy;
      wsize += cy;
    }
  w->_mp_size = usize >= 0 ? wsize : -wsize;
}
