/* mpq_cmp(u,v) -- Compare U, V.  Return postive, zero, or negative
   based on if U > V, U == V, or U < V.

Copyright (C) 1991, 1994, 1996 Free Software Foundation, Inc.

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
#include "longlong.h"

int
#if __STDC__
mpq_cmp (const MP_RAT *op1, const MP_RAT *op2)
#else
mpq_cmp (op1, op2)
     const MP_RAT *op1;
     const MP_RAT *op2;
#endif
{
  mp_size_t num1_size = op1->_mp_num._mp_size;
  mp_size_t den1_size = op1->_mp_den._mp_size;
  mp_size_t num2_size = op2->_mp_num._mp_size;
  mp_size_t den2_size = op2->_mp_den._mp_size;
  mp_size_t tmp1_size, tmp2_size;
  mp_ptr tmp1_ptr, tmp2_ptr;
  mp_size_t num1_sign;
  int cc;
  TMP_DECL (marker);

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

  /* 1. Check to see if we can tell which operand is larger by just looking at
     the number of limbs.  */

  /* NUM1 x DEN2 is either TMP1_SIZE limbs or TMP1_SIZE-1 limbs.
     Same for NUM1 x DEN1 with respect to TMP2_SIZE.  */
  if (tmp1_size > tmp2_size + 1)
    /* NUM1 x DEN2 is surely larger in magnitude than NUM2 x DEN1.  */
    return num1_sign;
  if (tmp2_size > tmp1_size + 1)
    /* NUM1 x DEN2 is surely smaller in magnitude than NUM2 x DEN1.  */
    return -num1_sign;

  /* 2. Same, but compare the number of significant bits.  */
  {
    int cnt1, cnt2;
    unsigned long int bits1, bits2;

    count_leading_zeros (cnt1, op1->_mp_num._mp_d[num1_size - 1]);
    count_leading_zeros (cnt2, op2->_mp_den._mp_d[den2_size - 1]);
    bits1 = tmp1_size * BITS_PER_MP_LIMB - cnt1 - cnt2;

    count_leading_zeros (cnt1, op2->_mp_num._mp_d[num2_size - 1]);
    count_leading_zeros (cnt2, op1->_mp_den._mp_d[den1_size - 1]);
    bits2 = tmp2_size * BITS_PER_MP_LIMB - cnt1 - cnt2;

    if (bits1 > bits2 + 1)
      return num1_sign;
    if (bits2 > bits1 + 1)
      return -num1_sign;
  }

  /* 3. Finally, cross multiply and compare.  */

  TMP_MARK (marker);
  tmp1_ptr = (mp_ptr) TMP_ALLOC (tmp1_size * BYTES_PER_MP_LIMB);
  tmp2_ptr = (mp_ptr) TMP_ALLOC (tmp2_size * BYTES_PER_MP_LIMB);

  if (num1_size >= den2_size)
    tmp1_size -= 0 == mpn_mul (tmp1_ptr,
			       op1->_mp_num._mp_d, num1_size,
			       op2->_mp_den._mp_d, den2_size);
  else
    tmp1_size -= 0 == mpn_mul (tmp1_ptr,
			       op2->_mp_den._mp_d, den2_size,
			       op1->_mp_num._mp_d, num1_size);

   if (num2_size >= den1_size)
     tmp2_size -= 0 == mpn_mul (tmp2_ptr,
				op2->_mp_num._mp_d, num2_size,
				op1->_mp_den._mp_d, den1_size);
   else
     tmp2_size -= 0 == mpn_mul (tmp2_ptr,
				op1->_mp_den._mp_d, den1_size,
				op2->_mp_num._mp_d, num2_size);


  cc = tmp1_size - tmp2_size != 0
    ? tmp1_size - tmp2_size : mpn_cmp (tmp1_ptr, tmp2_ptr, tmp1_size);
  TMP_FREE (marker);
  return num1_sign < 0 ? -cc : cc;
}
