/* mpz_get_si(integer) -- Return the least significant digit from INTEGER.

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

signed long int
#ifdef __STDC__
mpz_get_si (const MP_INT *integer)
#else
mpz_get_si (integer)
     const MP_INT *integer;
#endif
{
  mp_size size = integer->size;

  if (size > 0)
    return integer->d[0] % ((mp_limb) 1 << (BITS_PER_MP_LIMB - 1));
  else if (size < 0)
    return -(integer->d[0] % ((mp_limb) 1 << (BITS_PER_MP_LIMB - 1)));
  else
    return 0;
}
