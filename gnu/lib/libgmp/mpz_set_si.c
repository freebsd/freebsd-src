/* mpz_set_si(integer, val) -- Assign INTEGER with a small value VAL.

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
mpz_set_si (MP_INT *dest, signed long int val)
#else
mpz_set_si (dest, val)
     MP_INT *dest;
     signed long int val;
#endif
{
  /* We don't check if the allocation is enough, since the rest of the
     package ensures it's at least 1, which is what we need here.  */
  if (val > 0)
    {
      dest->d[0] = val;
      dest->size = 1;
    }
  else if (val < 0)
    {
      dest->d[0] = -val;
      dest->size = -1;
    }
  else
    dest->size = 0;
}
