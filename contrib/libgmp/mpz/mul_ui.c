/* mpz_mul_ui(product, multiplier, small_multiplicand) -- Set
   PRODUCT to MULTIPLICATOR times SMALL_MULTIPLICAND.

Copyright (C) 1991, 1993, 1994, 1996 Free Software Foundation, Inc.

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
mpz_mul_ui (mpz_ptr prod, mpz_srcptr mult, unsigned long int small_mult)
#else
mpz_mul_ui (prod, mult, small_mult)
     mpz_ptr prod;
     mpz_srcptr mult;
     unsigned long int small_mult;
#endif
{
  mp_size_t size = mult->_mp_size;
  mp_size_t sign_product = size;
  mp_limb_t cy;
  mp_size_t prod_size;
  mp_ptr prod_ptr;

  size = ABS (size);

  if (size == 0 || small_mult == 0)
    {
      prod->_mp_size = 0;
      return;
    }

  prod_size = size + 1;
  if (prod->_mp_alloc < prod_size)
    _mpz_realloc (prod, prod_size);

  prod_ptr = prod->_mp_d;

  cy = mpn_mul_1 (prod_ptr, mult->_mp_d, size, (mp_limb_t) small_mult);
  if (cy != 0)
    {
      prod_ptr[size] = cy;
      size++;
    }

  prod->_mp_size = sign_product >= 0 ? size : -size;
}
