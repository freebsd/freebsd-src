/* mpq_set_ui(dest,ulong_num,ulong_den) -- Set DEST to the retional number
   ULONG_NUM/ULONG_DEN.

Copyright (C) 1991, 1994, 1995 Free Software Foundation, Inc.

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
mpq_set_ui (MP_RAT *dest, unsigned long int num, unsigned long int den)
#else
mpq_set_ui (dest, num, den)
     MP_RAT *dest;
     unsigned long int num;
     unsigned long int den;
#endif
{
  if (num == 0)
    {
      /* Canonicalize 0/n to 0/1.  */
      den = 1;
      dest->_mp_num._mp_size = 0;
    }
  else
    {
      dest->_mp_num._mp_d[0] = num;
      dest->_mp_num._mp_size = 1;
    }

  dest->_mp_den._mp_d[0] = den;
  dest->_mp_den._mp_size = 1;
}
