/* mpz_pow_ui(res, base, exp) -- Set RES to BASE**EXP.

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
#include "longlong.h"

#ifndef BERKELEY_MP
void
#ifdef __STDC__
mpz_pow_ui (MP_INT *r, const MP_INT *b, unsigned long int e)
#else
mpz_pow_ui (r, b, e)
     MP_INT *r;
     const MP_INT *b;
     unsigned long int e;
#endif
#else /* BERKELEY_MP */
void
#ifdef __STDC__
rpow (const MP_INT *b, signed short int e, MP_INT *r)
#else
rpow (b, e, r)
     const MP_INT *b;
     signed short int e;
     MP_INT *r;
#endif
#endif /* BERKELEY_MP */
{
  mp_ptr rp, bp, tp, xp;
  mp_size rsize, bsize;
  int cnt, i;

  bsize = ABS (b->size);

  /* Single out cases that give result == 0 or 1.  These tests are here
     to simplify the general code below, not to optimize.  */
  if (bsize == 0
#ifdef BERKELEY_MP
      || e < 0
#endif
      )
    {
      r->size = 0;
      return;
    }
  if (e == 0)
    {
      r->d[0] = 1;
      r->size = 1;
      return;
    }

  /* Count the number of leading zero bits of the base's most
     significant limb.  */
  count_leading_zeros (cnt, b->d[bsize - 1]);

  /* Over-estimate space requirements and allocate enough space for the
     final result in two temporary areas.  The two areas are used to
     alternately hold the input and recieve the product for mpn_mul.
     (This scheme is used to fulfill the requirements of mpn_mul; that
     the product space may not be the same as any of the input operands.)  */
  rsize = bsize * e - cnt * e / BITS_PER_MP_LIMB;

  rp = (mp_ptr) alloca (rsize * BYTES_PER_MP_LIMB);
  tp = (mp_ptr) alloca (rsize * BYTES_PER_MP_LIMB);
  bp = b->d;

  MPN_COPY (rp, bp, bsize);
  rsize = bsize;
  count_leading_zeros (cnt, e);

  for (i = BITS_PER_MP_LIMB - cnt - 2; i >= 0; i--)
    {
      rsize = mpn_mul (tp, rp, rsize, rp, rsize);
      xp = tp; tp = rp; rp = xp;

      if ((e & ((mp_limb) 1 << i)) != 0)
	{
	  rsize = mpn_mul (tp, rp, rsize, bp, bsize);
	  xp = tp; tp = rp; rp = xp;
	}
    }

  /* Now then we know the exact space requirements, reallocate if
     necessary.  */
  if (r->alloc < rsize)
    _mpz_realloc (r, rsize);

  MPN_COPY (r->d, rp, rsize);
  r->size = (e & 1) == 0 || b->size >= 0 ? rsize : -rsize;
  alloca (0);
}
