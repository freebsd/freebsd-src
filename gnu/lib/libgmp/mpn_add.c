/* mpn_add -- Add two low-level integers.

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

/* Add ADD1_PTR/ADD1_SIZE and ADD2_PTR/ADD2_SIZE and store the first
   ADD1_SIZE words of the result at SUM_PTR.

   Return 1 if carry out was generated, return 0 otherwise.

   Argument constraint: ADD1_SIZE >= ADD2_SIZE.

   The size of SUM can be calculated as ADD1_SIZE + the return value.  */

mp_limb
#ifdef __STDC__
mpn_add (mp_ptr sum_ptr,
	 mp_srcptr add1_ptr, mp_size add1_size,
	 mp_srcptr add2_ptr, mp_size add2_size)
#else
mpn_add (sum_ptr, add1_ptr, add1_size, add2_ptr, add2_size)
     mp_ptr sum_ptr;
     mp_srcptr add1_ptr;
     mp_size add1_size;
     mp_srcptr add2_ptr;
     mp_size add2_size;
#endif
{
  mp_limb a1, a2, sum;
  mp_size j;

  /* The loop counter and index J goes from some negative value to zero.
     This way the loops become faster.  Need to offset the base pointers
     to take care of the negative indices.  */

  j = -add2_size;
  if (j == 0)
    goto add2_finished;

  add1_ptr -= j;
  add2_ptr -= j;
  sum_ptr -= j;

  /* There are two do-loops, marked NON-CARRY LOOP and CARRY LOOP that
     jump between each other.  The first loop is for when the previous
     addition didn't produce a carry-out; the second is for the
     complementary case.  */

  /* NON-CARRY LOOP */
  do
    {
      a1 = add1_ptr[j];
      a2 = add2_ptr[j];
      sum = a1 + a2;
      sum_ptr[j] = sum;
      if (sum < a2)
	goto cy_loop;
    ncy_loop:
      j++;
    }
  while (j < 0);

  /* We have exhausted ADD2.  Just copy ADD1 to SUM, and return
     0 as an indication of no carry-out.  */

 add2_finished:
  /* Immediate return if the copy would be a no-op.  */
  if (sum_ptr == add1_ptr)
    return 0;

  j = add2_size - add1_size;
  add1_ptr -= j;
  sum_ptr -= j;

  while (j < 0)
    {
      sum_ptr[j] = add1_ptr[j];
      j++;
    }
  return 0;

  /* CARRY LOOP */
  do
    {
      a1 = add1_ptr[j];
      a2 = add2_ptr[j];
      sum = a1 + a2 + 1;
      sum_ptr[j] = sum;
      if (sum > a2)
	goto ncy_loop;
    cy_loop:
      j++;
    }
  while (j < 0);

  j = add2_size - add1_size;
  add1_ptr -= j;
  sum_ptr -= j;

  while (j < 0)
    {
      a1 = add1_ptr[j];
      sum = a1 + 1;
      sum_ptr[j] = sum;
      if (sum > 0)
	goto copy_add1;
      j++;
    }
  return 1;

 copy_add1:
  if (sum_ptr == add1_ptr)
    return 0;

  j++;
  while (j < 0)
    {
      sum_ptr[j] = add1_ptr[j];
      j++;
    }

  return 0;
}
