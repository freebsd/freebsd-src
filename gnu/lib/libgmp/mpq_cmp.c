/* mpq_cmp(u,v) -- Compare U, V.  Return positive, zero, or negative
   based on if U > V, U == V, or U < V.

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

int
#ifdef __STDC__
mpq_cmp (const MP_RAT *op1, const MP_RAT *op2)
#else
mpq_cmp (op1, op2)
     const MP_RAT *op1;
     const MP_RAT *op2;
#endif
{
  mp_size num1_size = op1->num.size;
  mp_size den1_size = op1->den.size;
  mp_size num2_size = op2->num.size;
  mp_size den2_size = op2->den.size;
  mp_size tmp1_size, tmp2_size;
  mp_ptr tmp1_ptr, tmp2_ptr;
  mp_size num1_sign;
  int cc;

  if (num1_size == 0)
    return -num2_size;
  if (num2_size == 0)
    return num1_size;
  if ((num1_size ^ num2_size) < 0) /* I.e. are the signs different? */
    return num1_size;

  num1_sign = num1_size;
  num1_size = ABS (num1_size);
  num2_size = ABS (num2_size);

  tmp1_size = num1_size + den2_size;
  tmp2_size = num2_size + den1_size;

  if (tmp1_size != tmp2_size)
    return (tmp1_size - tmp2_size) ^ num1_sign;

  tmp1_ptr = (mp_ptr) alloca (tmp1_size * BYTES_PER_MP_LIMB);
  tmp2_ptr = (mp_ptr) alloca (tmp2_size * BYTES_PER_MP_LIMB);

  tmp1_size = (num1_size >= den2_size)
    ? mpn_mul (tmp1_ptr, op1->num.d, num1_size, op2->den.d, den2_size)
    : mpn_mul (tmp1_ptr, op2->den.d, den2_size, op1->num.d, num1_size);

  tmp2_size = (num2_size >= den1_size)
    ? mpn_mul (tmp2_ptr, op2->num.d, num2_size, op1->den.d, den1_size)
    : mpn_mul (tmp2_ptr, op1->den.d, den1_size, op2->num.d, num2_size);

  cc = tmp1_size - tmp2_size != 0
    ? tmp1_size - tmp2_size : mpn_cmp (tmp1_ptr, tmp2_ptr, tmp1_size);

  alloca (0);
  return (num1_sign < 0) ? -cc : cc;
}
