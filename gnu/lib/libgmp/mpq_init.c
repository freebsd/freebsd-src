/* mpq_init -- Make a new rational number with value 0/1.

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
mpq_init (MP_RAT *x)
#else
mpq_init (x)
     MP_RAT *x;
#endif
{
  x->num.alloc = 1;
  x->num.d = (mp_ptr) (*_mp_allocate_func) (BYTES_PER_MP_LIMB * x->num.alloc);
  x->num.size = 0;
  x->den.alloc = 1;
  x->den.d = (mp_ptr) (*_mp_allocate_func) (BYTES_PER_MP_LIMB * x->den.alloc);
  x->den.d[0] = 1;
  x->den.size = 1;
}
