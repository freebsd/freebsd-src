/* mpf_cmp_ui -- Compare a float with an unsigned integer.

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
mpf_cmp_ui (mpf_srcptr u, unsigned long int vlimb)
#else
mpf_cmp_ui (u, vlimb)
     mpf_srcptr u;
     unsigned long int vlimb;
#endif
{
  mp_srcptr up;
  mp_size_t usize;
  mp_exp_t uexp;

  uexp = u->_mp_exp;
  usize = u->_mp_size;

  /* 1. Is U negative?  */
  if (usize < 0)
    return -1;
  /* We rely on usize being non-negative in the code that follows.  */

  if (vlimb == 0)
    return usize != 0;

  /* 2. Are the exponents different (V's exponent == 1)?  */
  if (uexp > 1)
    return 1;
  if (uexp < 1)
    return -1;

  up = u->_mp_d;

#define STRICT_MPF_NORMALIZATION 0
#if ! STRICT_MPF_NORMALIZATION
  /* Ignore zeroes at the low end of U.  */
  while (*up == 0)
    {
      up++;
      usize--;
    }
#endif

  /* 3. Now, if the number of limbs are different, we have a difference
     since we have made sure the trailing limbs are not zero.  */
  if (usize > 1)
    return 1;

  /* 4. Compare the mantissas.  */
  if (*up > vlimb)
    return 1;
  else if (*up < vlimb)
    return -1;

  /* Wow, we got zero even if we tried hard to avoid it.  */
  return 0;
}
