/* mpz_cmp_ui.c -- Compare a MP_INT a with an mp_limb b.  Return positive,
  zero, or negative based on if a > b, a == b, or a < b.

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
mpz_cmp_ui (const MP_INT *u, mp_limb v_digit)
#else
mpz_cmp_ui (u, v_digit)
     const MP_INT *u;
     mp_limb v_digit;
#endif
{
  mp_size usize = u->size;

  if (usize == 0)
    return -(v_digit != 0);

  if (usize == 1)
    {
      mp_limb u_digit;

      u_digit = u->d[0];
      if (u_digit > v_digit)
	return 1;
      if (u_digit < v_digit)
	return -1;
      return 0;
    }

  return (usize > 0) ? 1 : -1;
}
