/* double mpf_get_d (mpf_t src) -- Return the double approximation to SRC.

Copyright (C) 1996 Free Software Foundation, Inc.

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

double
#if __STDC__
mpf_get_d (mpf_srcptr src)
#else
mpf_get_d (src)
     mpf_srcptr src;
#endif
{
  double res;
  mp_size_t size, i, n_limbs_to_use;
  int negative;
  mp_ptr qp;

  size = SIZ(src);
  if (size == 0)
    return 0.0;

  negative = size < 0;
  size = ABS (size);
  qp = PTR(src);

  res = qp[size - 1];
  n_limbs_to_use = MIN (LIMBS_PER_DOUBLE, size);
  for (i = 2; i <= n_limbs_to_use; i++)
    res = res * MP_BASE_AS_DOUBLE + qp[size - i];

  res = __gmp_scale2 (res, (EXP(src) - n_limbs_to_use) * BITS_PER_MP_LIMB);

  return negative ? -res : res;
}
