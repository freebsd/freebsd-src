/* mpf_set_prec_raw(x,bits) -- Change the precision of x without changing
   allocation.  For proper operation, the original precision need to be reset
   sooner or later.

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
mpf_set_prec_raw (mpf_ptr x, unsigned long int prec_in_bits)
#else
mpf_set_prec_raw (x, prec_in_bits)
     mpf_ptr x;
     unsigned long int prec_in_bits;
#endif
{
  mp_size_t prec;
  prec = (MAX (53, prec_in_bits) + 2 * BITS_PER_MP_LIMB - 1)/BITS_PER_MP_LIMB;
  x->_mp_prec = prec;
}
