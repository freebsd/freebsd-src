/* mpz_set (dest_integer, src_integer) -- Assign DEST_INTEGER from SRC_INTEGER.

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
mpz_set (MP_INT *w, const MP_INT *u)
#else
mpz_set (w, u)
     MP_INT *w;
     const MP_INT *u;
#endif
{
  mp_size usize;
  mp_size abs_usize;

  usize = u->size;
  abs_usize = ABS (usize);

  /* If not space for sum (and possible carry), increase space.  */
  if (w->alloc < abs_usize)
    _mpz_realloc (w, abs_usize);

  w->size = usize;
  MPN_COPY (w->d, u->d, abs_usize);
}
