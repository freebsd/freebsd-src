/* mpz_sub_ui -- Subtract an unsigned one-word integer from an MP_INT.

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
mpz_sub_ui (MP_INT *dif, const MP_INT *min, mp_limb sub)
#else
mpz_sub_ui (dif, min, sub)
     MP_INT *dif;
     const MP_INT *min;
     mp_limb sub;
#endif
{
  mp_srcptr minp;
  mp_ptr difp;
  mp_size minsize, difsize;
  mp_size abs_minsize;

  minsize = min->size;
  abs_minsize = ABS (minsize);

  /* If not space for SUM (and possible carry), increase space.  */
  difsize = abs_minsize + 1;
  if (dif->alloc < difsize)
    _mpz_realloc (dif, difsize);

  /* These must be after realloc (ADD1 may be the same as SUM).  */
  minp = min->d;
  difp = dif->d;

  if (sub == 0)
    {
      MPN_COPY (difp, minp, abs_minsize);
      dif->size = minsize;
      return;
    }
  if (abs_minsize == 0)
    {
      difp[0] = sub;
      dif->size = -1;
      return;
    }

  if (minsize < 0)
    {
      difsize = mpn_add (difp, minp, abs_minsize, &sub, 1);
      if (difsize != 0)
	difp[abs_minsize] = 1;
      difsize = -(difsize + abs_minsize);
    }
  else
    {
      /* The signs are different.  Need exact comparision to determine
	 which operand to subtract from which.  */
      if (abs_minsize == 1 && minp[0] < sub)
	difsize = -(abs_minsize
		    + mpn_sub (difp, &sub, 1, minp, 1));
      else
	difsize = (abs_minsize
		   + mpn_sub (difp, minp, abs_minsize, &sub, 1));
    }

  dif->size = difsize;
}
