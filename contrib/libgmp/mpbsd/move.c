/* move -- BSD compatible assignment.

Copyright (C) 1991, 1994, 1995 Free Software Foundation, Inc.

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

#include "mp.h"
#include "gmp.h"
#include "gmp-impl.h"

void
#if __STDC__
move (const MINT *u, MINT *w)
#else
move (u, w)
     const MINT *u;
     MINT *w;
#endif
{
  mp_size_t usize;
  mp_size_t abs_usize;

  usize = u->_mp_size;
  abs_usize = ABS (usize);

  if (w->_mp_alloc < abs_usize)
    _mp_realloc (w, abs_usize);

  w->_mp_size = usize;
  MPN_COPY (w->_mp_d, u->_mp_d, abs_usize);
}
