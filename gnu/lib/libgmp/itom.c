/* itom -- BSD compatible allocate and initiate a MINT.

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

#include "mp.h"
#include "gmp.h"
#include "gmp-impl.h"

MINT *
#ifdef __STDC__
itom (signed short int n)
#else
itom (n)
     short int n;
#endif
{
  MINT *x;
  mp_ptr xp;

  x = (MINT *) (*_mp_allocate_func) (sizeof (MINT));
  x->alloc = 1;
  x->d = xp = (mp_ptr) (*_mp_allocate_func) (x->alloc * BYTES_PER_MP_LIMB);
  if (n > 0)
    {
      x->size = 1;
      xp[0] = n;
    }
  else if (n < 0)
    {
      x->size = -1;
      xp[0] = -n;
    }
  else
    x->size = 0;

  return x;
}
