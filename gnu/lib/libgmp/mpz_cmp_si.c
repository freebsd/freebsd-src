/* mpz_cmp_si(u,v) -- Compare an integer U with a single-word int V.
   Return positive, zero, or negative based on if U > V, U == V, or U < V.

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

int
#ifdef __STDC__
mpz_cmp_si (const MP_INT *u, signed long int v_digit)
#else
mpz_cmp_si (u, v_digit)
     const MP_INT *u;
     signed long int v_digit;
#endif
{
  mp_size usize = u->size;
  mp_size vsize;
  mp_limb u_digit;

  vsize = 0;
  if (v_digit > 0)
    vsize = 1;
  else if (v_digit < 0)
    {
      vsize = -1;
      v_digit = -v_digit;
    }

  if (usize != vsize)
    return usize - vsize;

  if (usize == 0)
    return 0;

  u_digit = u->d[0];

  if (u_digit == v_digit)
    return 0;

  if ((u_digit < v_digit) == (usize < 0))
    return 1;
  else
    return -1;
}
