/* __gmp_insert_double -- convert from array of mp_limb_t to double.

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

#ifdef XDEBUG
#undef _GMP_IEEE_FLOATS
#endif

#ifndef _GMP_IEEE_FLOATS
#define _GMP_IEEE_FLOATS 0
#endif

double
#if __STDC__
__gmp_scale2 (double d, int exp)
#else
__gmp_scale2 (d, exp)
     double d;
     int exp;
#endif
{
#if _GMP_IEEE_FLOATS
  {
    union ieee_double_extract x;
    x.d = d;
    x.s.exp += exp;
    return x.d;
  }
#else
  {
    double factor, r;

    factor = 2.0;
    if (exp < 0)
      {
	factor = 0.5;
	exp = -exp;
      }
    r = d;
    while (exp != 0)
      {
	if ((exp & 1) != 0)
	  r *= factor;
	factor *= factor;
	exp >>= 1;
      }
    return r;
  }
#endif
}
