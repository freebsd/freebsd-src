/* mpz_add_ui -- Add an MP_INT and an unsigned one-word integer.

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
mpz_add_ui (MP_INT *sum, const MP_INT *add1, mp_limb add2)
#else
mpz_add_ui (sum, add1, add2)
     MP_INT *sum;
     const MP_INT *add1;
     mp_limb add2;
#endif
{
  mp_srcptr add1p;
  mp_ptr sump;
  mp_size add1size, sumsize;
  mp_size abs_add1size;

  add1size = add1->size;
  abs_add1size = ABS (add1size);

  /* If not space for SUM (and possible carry), increase space.  */
  sumsize = abs_add1size + 1;
  if (sum->alloc < sumsize)
    _mpz_realloc (sum, sumsize);

  /* These must be after realloc (ADD1 may be the same as SUM).  */
  add1p = add1->d;
  sump = sum->d;

  if (add2 == 0)
    {
      MPN_COPY (sump, add1p, abs_add1size);
      sum->size = add1size;
      return;
    }
  if (abs_add1size == 0)
    {
      sump[0] = add2;
      sum->size = 1;
      return;
    }

  if (add1size >= 0)
    {
      sumsize = mpn_add (sump, add1p, abs_add1size, &add2, 1);
      if (sumsize != 0)
	sump[abs_add1size] = 1;
      sumsize = sumsize + abs_add1size;
    }
  else
    {
      /* The signs are different.  Need exact comparision to determine
	 which operand to subtract from which.  */
      if (abs_add1size == 1 && add1p[0] < add2)
	sumsize = (abs_add1size
		   + mpn_sub (sump, &add2, 1, add1p, 1));
      else
	sumsize = -(abs_add1size
		    + mpn_sub (sump, add1p, abs_add1size, &add2, 1));
    }

  sum->size = sumsize;
}
