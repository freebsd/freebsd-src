/* mpz_mul_ui(product, multiplier, small_multiplicand) -- Set
   PRODUCT to MULTIPLICATOR times SMALL_MULTIPLICAND.

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
#include "longlong.h"

void
#ifdef __STDC__
mpz_mul_ui (MP_INT *prod, const MP_INT *mult,
	    unsigned long int small_mult)
#else
mpz_mul_ui (prod, mult, small_mult)
     MP_INT *prod;
     const MP_INT *mult;
     unsigned long int small_mult;
#endif
{
  mp_size mult_size = mult->size;
  mp_size sign_product = mult_size;
  mp_size i;
  mp_limb cy;
  mp_size prod_size;
  mp_srcptr mult_ptr;
  mp_ptr prod_ptr;

  mult_size = ABS (mult_size);

  if (mult_size == 0 || small_mult == 0)
    {
      prod->size = 0;
      return;
    }

  prod_size = mult_size + 1;
  if (prod->alloc < prod_size)
    _mpz_realloc (prod, prod_size);

  mult_ptr = mult->d;
  prod_ptr = prod->d;

  cy = 0;
  for (i = 0; i < mult_size; i++)
    {
      mp_limb p1, p0;
      umul_ppmm (p1, p0, small_mult, mult_ptr[i]);
      p0 += cy;
      cy = p1 + (p0 < cy);
      prod_ptr[i] = p0;
    }

  prod_size = mult_size;
  if (cy != 0)
    {
      prod_ptr[mult_size] = cy;
      prod_size++;
    }

  prod->size = sign_product > 0 ? prod_size : -prod_size;
}
