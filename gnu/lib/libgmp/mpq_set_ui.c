/* mpq_set_ui(dest,ulong_num,ulong_den) -- Set DEST to the retional number
   ULONG_NUM/ULONG_DEN.

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

static unsigned long int
gcd (x, y)
     unsigned long int x, y;
{
  for (;;)
    {
      x = x % y;
      if (x == 0)
	return y;
      y = y % x;
      if (y == 0)
	return x;
    }
}

void
#ifdef __STDC__
mpq_set_ui (MP_RAT *dest, unsigned long int num, unsigned long int den)
#else
mpq_set_ui (dest, num, den)
     MP_RAT *dest;
     unsigned long int num;
     unsigned long int den;
#endif
{
  unsigned long int g;

  if (num == 0)
    {
      /* Canonicalize 0/n to 0/1.  */
      den = 1;
      dest->num.size = 0;
    }
  else
    {
      /* Remove any common factor in NUM and DEN. */
      /* Pass DEN as the second argument to gcd, in order to make the
	 gcd function divide by zero if DEN is zero.  */
      g = gcd (num, den);
      num /= g;
      den /= g;

      dest->num.d[0] = num;
      dest->num.size = 1;
    }

  dest->den.d[0] = den;
  dest->den.size = 1;
}
