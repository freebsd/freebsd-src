/* mpf_set_z -- Assign a float from an integer.

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
mpf_set_z (mpf_ptr r, mpz_srcptr u)
#else
mpf_set_z (r, u)
     mpf_ptr r;
     mpz_srcptr u;
#endif
{
  mp_ptr rp, up;
  mp_size_t size, asize;
  mp_size_t prec;

  prec = PREC (r) + 1;
  size = SIZ (u);
  asize = ABS (size);
  rp = PTR (r);
  up = PTR (u);

  EXP (r) = asize;

  if (asize > prec)
    {
      up += asize - prec;
      asize = prec;
    }

  MPN_COPY (rp, up, asize);
  SIZ (r) = size >= 0 ? asize : -asize;
}
