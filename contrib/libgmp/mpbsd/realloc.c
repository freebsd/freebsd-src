/* _mp_realloc -- make the MINT* have NEW_SIZE digits allocated.

Copyright (C) 1991, 1993, 1994, 1995 Free Software Foundation, Inc.

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

void *
#if __STDC__
_mp_realloc (MINT *m, mp_size_t new_size)
#else
_mp_realloc (m, new_size)
     MINT *m;
     mp_size_t new_size;
#endif
{
  /* Never allocate zero space. */
  if (new_size == 0)
    new_size = 1;

  m->_mp_d = (mp_ptr) (*_mp_reallocate_func) (m->_mp_d,
					      m->_mp_alloc * BYTES_PER_MP_LIMB,
					      new_size * BYTES_PER_MP_LIMB);
  m->_mp_alloc = new_size;
  return (void *) m->_mp_d;
}
