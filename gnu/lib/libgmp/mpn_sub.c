/* mpn_sub -- Subtract two low-level natural-number integers.

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

/* Subtract SUB_PTR/SUB_SIZE from MIN_PTR/MIN_SIZE and store the
   result (MIN_SIZE words) at DIF_PTR.

   Return 1 if min < sub (result is negative).  Otherwise, return the
   negative difference between the number of words in dif and min.
   (I.e.  return 0 if the result has MIN_SIZE words, -1 if it has
   MIN_SIZE - 1 words, etc.)

   Argument constraint: MIN_SIZE >= SUB_SIZE.

   The size of DIF can be calculated as MIN_SIZE + the return value.  */

mp_size
#ifdef __STDC__
mpn_sub (mp_ptr dif_ptr,
	 mp_srcptr min_ptr, mp_size min_size,
	 mp_srcptr sub_ptr, mp_size sub_size)
#else
mpn_sub (dif_ptr, min_ptr, min_size, sub_ptr, sub_size)
     mp_ptr dif_ptr;
     mp_srcptr min_ptr;
     mp_size min_size;
     mp_srcptr sub_ptr;
     mp_size sub_size;
#endif
{
  mp_limb m, s, dif;
  mp_size j;

  /* The loop counter and index J goes from some negative value to zero.
     This way the loops are faster.  Need to offset the base pointers
     to take care of the negative indices.  */

  j = -sub_size;
  if (j == 0)
    goto sub_finished;

  min_ptr -= j;
  sub_ptr -= j;
  dif_ptr -= j;

  /* There are two do-loops, marked NON-CARRY LOOP and CARRY LOOP that
     jump between each other.  The first loop is for when the previous
     subtraction didn't produce a carry-out; the second is for the
     complementary case.  */

  /* NON-CARRY LOOP */
  do
    {
      m = min_ptr[j];
      s = sub_ptr[j];
      dif = m - s;
      dif_ptr[j] = dif;
      if (dif > m)
	goto cy_loop;
    ncy_loop:
      j++;
    }
  while (j < 0);

  /* We have exhausted SUB, with no carry out.  Copy remaining part of
     MIN to DIF.  */

 sub_finished:
  j = sub_size - min_size;

  /* If there's no difference between the length of the operands, the
     last words might have become zero, and re-normalization is needed.  */
  if (j == 0)
    goto normalize;

  min_ptr -= j;
  dif_ptr -= j;

  goto copy;

  /* CARRY LOOP */
  do
    {
      m = min_ptr[j];
      s = sub_ptr[j];
      dif = m - s - 1;
      dif_ptr[j] = dif;
      if (dif < m)
	goto ncy_loop;
    cy_loop:
      j++;
    }
  while (j < 0);

  /* We have exhausted SUB, but need to propagate carry.  */

  j = sub_size - min_size;
  if (j == 0)
    return 1;			/* min < sub.  Flag it to the caller */

  min_ptr -= j;
  dif_ptr -= j;

  /* Propagate carry.  Sooner or later the carry will cancel with a
     non-zero word, because the minuend is normalized.  Considering this,
     there's no need to test the index J.  */
  for (;;)
    {
      m = min_ptr[j];
      dif = m - 1;
      dif_ptr[j] = dif;
      j++;
      if (dif < m)
	break;
    }

  if (j == 0)
    goto normalize;

 copy:
  /* Don't copy the remaining words of MIN to DIF if MIN_PTR and DIF_PTR
     are equal.  It would just be a no-op copying.  Return 0, as the length
     of the result equals that of the minuend.  */
  if (dif_ptr == min_ptr)
    return 0;

  do
    {
      dif_ptr[j] = min_ptr[j];
      j++;
    }
  while (j < 0);
  return 0;

 normalize:
  for (j = -1; j >= -min_size; j--)
    {
      if (dif_ptr[j] != 0)
	return j + 1;
    }

  return -min_size;
}
