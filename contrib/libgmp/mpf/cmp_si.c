/* mpf_cmp_si -- Compare a float with a signed integer.

Copyright (C) 1993, 1994, 1995 Free Software Foundation, Inc.

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

int
#if __STDC__
mpf_cmp_si (mpf_srcptr u, long int vslimb)
#else
mpf_cmp_si (u, vslimb)
     mpf_srcptr u;
     long int vslimb;
#endif
{
  mp_srcptr up;
  mp_size_t usize;
  mp_exp_t uexp;
  int usign;

  uexp = u->_mp_exp;
  usize = u->_mp_size;

  /* 1. Are the signs different?  */
  if ((usize < 0) == (vslimb < 0)) /* don't use xor, type size may differ */
    {
      /* U and V are both non-negative or both negative.  */
      if (usize == 0)
	/* vslimb >= 0 */
	return -(vslimb != 0);
      if (vslimb == 0)
	/* usize >= 0 */
	return usize != 0;
      /* Fall out.  */
    }
  else
    {
      /* Either U or V is negative, but not both.  */
      return usize >= 0 ? 1 : -1;
    }

  /* U and V have the same sign and are both non-zero.  */

  usign = usize >= 0 ? 1 : -1;

  /* 2. Are the exponents different (V's exponent == 1)?  */
  if (uexp > 1)
    return usign;
  if (uexp < 1)
    return -usign;

  usize = ABS (usize);
  vslimb = ABS (vslimb);

  up = u->_mp_d;

#define STRICT_MPF_NORMALIZATION 0
#if ! STRICT_MPF_NORMALIZATION
  /* Ignore zeroes at the low end of U and V.  */
  while (*up == 0)
    {
      up++;
      usize--;
    }
#endif

  /* 3. Now, if the number of limbs are different, we have a difference
     since we have made sure the trailing limbs are not zero.  */
  if (usize > 1)
    return usign;

  /* 4. Compare the mantissas.  */
  if (*up > vslimb)
    return usign;
  else if (*up < vslimb)
    return -usign;

  /* Wow, we got zero even if we tried hard to avoid it.  */
  return 0;
}
