/* mpz_divexact -- finds quotient when known that quot * den == num && den != 0.

Copyright (C) 1991, 1993, 1994, 1995 Free Software Foundation, Inc.

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
MA 02111-1307, USA.  */

/*  Ken Weber (kweber@mat.ufrgs.br, kweber@mcs.kent.edu)

    Funding for this work has been partially provided by Conselho Nacional
    de Desenvolvimento Cienti'fico e Tecnolo'gico (CNPq) do Brazil, Grant
    301314194-2, and was done while I was a visiting reseacher in the Instituto
    de Matema'tica at Universidade Federal do Rio Grande do Sul (UFRGS).

    References:
        T. Jebelean, An algorithm for exact division, Journal of Symbolic
        Computation, v. 15, 1993, pp. 169-180.  */

#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

void
#if __STDC__
mpz_divexact (mpz_ptr quot, mpz_srcptr num, mpz_srcptr den)
#else
mpz_divexact (quot, num, den)
     mpz_ptr quot;
     mpz_srcptr num;
     mpz_srcptr den;
#endif
{
  mp_ptr qp, tp;
  mp_size_t qsize, tsize;

  mp_srcptr np = num->_mp_d;
  mp_srcptr dp = den->_mp_d;
  mp_size_t nsize = ABS (num->_mp_size);
  mp_size_t dsize = ABS (den->_mp_size);
  TMP_DECL (marker);

  /*  Generate divide-by-zero error if dsize == 0.  */
  if (dsize == 0)
    {
      quot->_mp_size = 1 / dsize;
      return;
    }

  if (nsize == 0)
    {
      quot->_mp_size = 0;
      return;
    }

  qsize = nsize - dsize + 1;
  if (quot->_mp_alloc < qsize)
    _mpz_realloc (quot, qsize);
  qp = quot->_mp_d;

  TMP_MARK (marker);

  /*  QUOT <-- NUM/2^r, T <-- DEN/2^r where = r number of twos in DEN.  */
  while (dp[0] == 0)
    np += 1, nsize -= 1, dp += 1, dsize -= 1;
  tsize = MIN (qsize, dsize);
  if (dp[0] & 1)
    {
      if (qp != dp)
	MPN_COPY (qp, np, qsize);
      if (qp == dp)		/*  QUOT and DEN overlap.  */
	{
	  tp = (mp_ptr) TMP_ALLOC (sizeof (mp_limb_t) * tsize);
	  MPN_COPY (tp, dp, tsize);
	}
      else
	tp = (mp_ptr) dp;
    }
  else
    {
      unsigned long int r;
      tp = (mp_ptr) TMP_ALLOC (sizeof (mp_limb_t) * tsize);
      count_trailing_zeros (r, dp[0]);
      mpn_rshift (tp, dp, tsize, r);
      if (dsize > tsize)
	tp[tsize-1] |= dp[tsize] << (BITS_PER_MP_LIMB - r);
      mpn_rshift (qp, np, qsize, r);
      if (nsize > qsize)
	qp[qsize-1] |= np[qsize] << (BITS_PER_MP_LIMB - r);
    }

  /*  Now QUOT <-- QUOT/T.  */
  mpn_bdivmod (qp, qp, qsize, tp, tsize, qsize * BITS_PER_MP_LIMB);
  MPN_NORMALIZE (qp, qsize);

  quot->_mp_size = (num->_mp_size < 0) == (den->_mp_size < 0) ? qsize : -qsize;

  TMP_FREE (marker);
}
