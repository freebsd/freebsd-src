/* xtom -- convert a hexadecimal string to a MINT, and return a pointer to
   the MINT.

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

#include "mp.h"
#include "gmp.h"
#include "gmp-impl.h"

MINT *
#ifdef __STDC__
xtom (const char *hex_str)
#else
xtom (hex_str)
     const char *hex_str;
#endif
{
  MINT *x = (MINT *) (*_mp_allocate_func) (sizeof (MINT));

  x->alloc = 1;
  x->d = (mp_ptr) (*_mp_allocate_func) (x->alloc * BYTES_PER_MP_LIMB);

  _mpz_set_str (x, hex_str, 16);
  return x;
}
