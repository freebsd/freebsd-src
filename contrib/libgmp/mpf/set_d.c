/* mpf_set_d -- Assign a float from a IEEE double.

Copyright (C) 1993, 1994, 1995, 1996 Free Software Foundation, Inc.

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
mpf_set_d (mpf_ptr r, double d)
#else
mpf_set_d (r, d)
     mpf_ptr r;
     double d;
#endif
{
  int negative;

  if (d == 0)
    {
      SIZ(r) = 0;
      EXP(r) = 0;
      return;
    }
  negative = d < 0;
  d = ABS (d);

  EXP(r) = __gmp_extract_double (PTR(r), d);
  SIZ(r) = negative ? -LIMBS_PER_DOUBLE : LIMBS_PER_DOUBLE;
}
