/* mpf_set_default_prec --

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

mp_size_t __gmp_default_fp_limb_precision
  = (53 + 2 * BITS_PER_MP_LIMB - 1) / BITS_PER_MP_LIMB;

void
#if __STDC__
mpf_set_default_prec (unsigned long int prec_in_bits)
#else
mpf_set_default_prec (prec_in_bits)
     unsigned long int prec_in_bits;
#endif
{
  mp_size_t prec;

  prec = (MAX (53, prec_in_bits) + 2 * BITS_PER_MP_LIMB - 1)/BITS_PER_MP_LIMB;
  __gmp_default_fp_limb_precision = prec;
}
