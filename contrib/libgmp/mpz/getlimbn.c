/* mpz_getlimbn(integer,n) -- Return the N:th limb from INTEGER.

Copyright (C) 1993, 1994, 1995 Free Software Foundation, Inc.

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

mp_limb_t
#if __STDC__
mpz_getlimbn (mpz_srcptr integer, mp_size_t n)
#else
mpz_getlimbn (integer, n)
     mpz_srcptr integer;
     mp_size_t n;
#endif
{
  if (integer->_mp_size <= n || n < 0)
    return 0;
  else
    return integer->_mp_d[n];
}
