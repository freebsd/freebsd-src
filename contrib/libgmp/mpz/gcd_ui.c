/* mpz_gcd_ui -- Calculate the greatest common divisior of two integers.

Copyright (C) 1994, 1996 Free Software Foundation, Inc.

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

unsigned long int
#if __STDC__
mpz_gcd_ui (mpz_ptr w, mpz_srcptr u, unsigned long int v)
#else
mpz_gcd_ui (w, u, v)
     mpz_ptr w;
     mpz_srcptr u;
     unsigned long int v;
#endif
{
  mp_size_t size;
  mp_limb_t res;

  size = ABS (u->_mp_size);

  if (size == 0)
    res = v;
  else if (v == 0)
    {
      if (w != NULL && u != w)
	{
	  if (w->_mp_alloc < size)
	    _mpz_realloc (w, size);

	  MPN_COPY (w->_mp_d, u->_mp_d, size);
	}
      w->_mp_size = size;
      /* We can't return any useful result for gcd(big,0).  */
      return size > 1 ? 0 : w->_mp_d[0];
    }
  else
    res = mpn_gcd_1 (u->_mp_d, size, v);

  if (w != NULL)
    {
      w->_mp_d[0] = res;
      w->_mp_size = 1;
    }
  return res;
}
