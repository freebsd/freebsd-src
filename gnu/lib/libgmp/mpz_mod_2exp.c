/* mpz_mod_2exp -- divide a MP_INT by 2**n and produce a remainder.

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
mpz_mod_2exp (MP_INT *res, const MP_INT *in, unsigned long int cnt)
#else
mpz_mod_2exp (res, in, cnt)
     MP_INT *res;
     const MP_INT *in;
     unsigned long int cnt;
#endif
{
  mp_size in_size = ABS (in->size);
  mp_size res_size;
  mp_size limb_cnt = cnt / BITS_PER_MP_LIMB;
  mp_srcptr in_ptr = in->d;

  if (in_size > limb_cnt)
    {
      /* The input operand is (probably) greater than 2**CNT.  */
      mp_limb x;

      x = in_ptr[limb_cnt] & (((mp_limb) 1 << cnt % BITS_PER_MP_LIMB) - 1);
      if (x != 0)
	{
	  res_size = limb_cnt + 1;
	  if (res->alloc < res_size)
	    _mpz_realloc (res, res_size);

	  res->d[limb_cnt] = x;
	}
      else
	{
	  mp_size i;

	  for (i = limb_cnt - 1; i >= 0; i--)
	    if (in_ptr[i] != 0)
	      break;
	  res_size = i + 1;

	  if (res->alloc < res_size)
	    _mpz_realloc (res, res_size);

	  limb_cnt = res_size;
	}
    }
  else
    {
      /* The input operand is smaller than 2**CNT.  We perform a no-op,
	 apart from that we might need to copy IN to RES.  */
      res_size = in_size;
      if (res->alloc < res_size)
	_mpz_realloc (res, res_size);

      limb_cnt = res_size;
    }

  if (res != in)
    MPN_COPY (res->d, in->d, limb_cnt);
  res->size = (in->size >= 0) ? res_size : -res_size;
}
