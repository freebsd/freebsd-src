/* mpz_tdiv_q_2exp -- Divide an integer by 2**CNT.  Round the quotient
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
mpz_tdiv_q_2exp (mpz_ptr w, mpz_srcptr u, unsigned long int cnt)
#else
mpz_tdiv_q_2exp (w, u, cnt)
     mpz_ptr w;
     mpz_srcptr u;
     unsigned long int cnt;
#endif
{
  mp_size_t usize, wsize;
  mp_size_t limb_cnt;

  usize = u->_mp_size;
  limb_cnt = cnt / BITS_PER_MP_LIMB;
  wsize = ABS (usize) - limb_cnt;
  if (wsize <= 0)
    w->_mp_size = 0;
  else
    {
      mp_ptr wp;
      mp_srcptr up;

      if (w->_mp_alloc < wsize)
	_mpz_realloc (w, wsize);

      wp = w->_mp_d;
      up = u->_mp_d;

      cnt %= BITS_PER_MP_LIMB;
      if (cnt != 0)
	{
	  mpn_rshift (wp, up + limb_cnt, wsize, cnt);
	  wsize -= wp[wsize - 1] == 0;
	}
      else
	{
	  MPN_COPY_INCR (wp, up + limb_cnt, wsize);
	}

      w->_mp_size = usize >= 0 ? wsize : -wsize;
    }
}
