/* mpz_set_f (dest_integer, src_float) -- Assign DEST_INTEGER from SRC_FLOAT.

Copyright (C) 1996 Free Software Foundation, Inc.

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
mpz_set_f (mpz_ptr w, mpf_srcptr u)
#else
mpz_set_f (w, u)
     mpz_ptr w;
     mpf_srcptr u;
#endif
{
  mp_ptr wp, up;
  mp_size_t usize, size;
  mp_exp_t exp;

  usize = SIZ (u);
  size = ABS (usize);
  exp = EXP (u);

  if (w->_mp_alloc < exp)
    _mpz_realloc (w, exp);

  wp = w->_mp_d;
  up = u->_mp_d;

  if (exp <= 0)
    {
      SIZ (w) = 0;
      return;
    }
  if (exp < size)
    {
      MPN_COPY (wp, up + size - exp, exp);
    }
  else
    {
      MPN_ZERO (wp, exp - size);
      MPN_COPY (wp + exp - size, up, size);
    }

  w->_mp_size = usize >= 0 ? exp : -exp;
}
