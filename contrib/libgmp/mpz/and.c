/* mpz_and -- Logical and.

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
mpz_and (mpz_ptr res, mpz_srcptr op1, mpz_srcptr op2)
#else
mpz_and (res, op1, op2)
     mpz_ptr res;
     mpz_srcptr op1;
     mpz_srcptr op2;
#endif
{
  mp_srcptr op1_ptr, op2_ptr;
  mp_size_t op1_size, op2_size;
  mp_ptr res_ptr;
  mp_size_t res_size;
  mp_size_t i;
  TMP_DECL (marker);

  TMP_MARK (marker);
  op1_size = op1->_mp_size;
  op2_size = op2->_mp_size;

  op1_ptr = op1->_mp_d;
  op2_ptr = op2->_mp_d;
  res_ptr = res->_mp_d;

  if (op1_size >= 0)
    {
      if (op2_size >= 0)
	{
	  res_size = MIN (op1_size, op2_size);
	  /* First loop finds the size of the result.  */
	  for (i = res_size - 1; i >= 0; i--)
	    if ((op1_ptr[i] & op2_ptr[i]) != 0)
	      break;
	  res_size = i + 1;

	  /* Handle allocation, now then we know exactly how much space is
	     needed for the result.  */
	  if (res->_mp_alloc < res_size)
	    {
	      _mpz_realloc (res, res_size);
	      op1_ptr = op1->_mp_d;
	      op2_ptr = op2->_mp_d;
	      res_ptr = res->_mp_d;
	    }

	  /* Second loop computes the real result.  */
	  for (i = res_size - 1; i >= 0; i--)
	    res_ptr[i] = op1_ptr[i] & op2_ptr[i];

	  res->_mp_size = res_size;
	  return;
	}
      else /* op2_size < 0 */
	{
	  /* Fall through to the code at the end of the function.  */
	}
    }
  else
    {
      if (op2_size < 0)
	{
	  mp_ptr opx;
	  mp_limb_t cy;
	  mp_size_t res_alloc;

	  /* Both operands are negative, so will be the result.
	     -((-OP1) & (-OP2)) = -(~(OP1 - 1) & ~(OP2 - 1)) =
	     = ~(~(OP1 - 1) & ~(OP2 - 1)) + 1 =
	     = ((OP1 - 1) | (OP2 - 1)) + 1      */

	  /* It might seem as we could end up with an (invalid) result with
	     a leading zero-limb here when one of the operands is of the
	     type 1,,0,,..,,.0.  But some analysis shows that we surely
	     would get carry into the zero-limb in this situation...  */

	  op1_size = -op1_size;
	  op2_size = -op2_size;

	  res_alloc = 1 + MAX (op1_size, op2_size);

	  opx = (mp_ptr) TMP_ALLOC (op1_size * BYTES_PER_MP_LIMB);
	  mpn_sub_1 (opx, op1_ptr, op1_size, (mp_limb_t) 1);
	  op1_ptr = opx;

	  opx = (mp_ptr) TMP_ALLOC (op2_size * BYTES_PER_MP_LIMB);
	  mpn_sub_1 (opx, op2_ptr, op2_size, (mp_limb_t) 1);
	  op2_ptr = opx;

	  if (res->_mp_alloc < res_alloc)
	    {
	      _mpz_realloc (res, res_alloc);
	      res_ptr = res->_mp_d;
	      /* Don't re-read OP1_PTR and OP2_PTR.  They point to
		 temporary space--never to the space RES->_mp_D used
		 to point to before reallocation.  */
	    }

	  if (op1_size >= op2_size)
	    {
	      MPN_COPY (res_ptr + op2_size, op1_ptr + op2_size,
			op1_size - op2_size);
	      for (i = op2_size - 1; i >= 0; i--)
		res_ptr[i] = op1_ptr[i] | op2_ptr[i];
	      res_size = op1_size;
	    }
	  else
	    {
	      MPN_COPY (res_ptr + op1_size, op2_ptr + op1_size,
			op2_size - op1_size);
	      for (i = op1_size - 1; i >= 0; i--)
		res_ptr[i] = op1_ptr[i] | op2_ptr[i];
	      res_size = op2_size;
	    }

	  cy = mpn_add_1 (res_ptr, res_ptr, res_size, (mp_limb_t) 1);
	  if (cy)
	    {
	      res_ptr[res_size] = cy;
	      res_size++;
	    }

	  res->_mp_size = -res_size;
	  TMP_FREE (marker);
	  return;
	}
      else
	{
	  /* We should compute -OP1 & OP2.  Swap OP1 and OP2 and fall
	     through to the code that handles OP1 & -OP2.  */
	  {mpz_srcptr t = op1; op1 = op2; op2 = t;}
	  {mp_srcptr t = op1_ptr; op1_ptr = op2_ptr; op2_ptr = t;}
	  {mp_size_t t = op1_size; op1_size = op2_size; op2_size = t;}
	}

    }

  {
#if ANDNEW
    mp_size_t op2_lim;
    mp_size_t count;

    /* OP2 must be negated as with infinite precision.

       Scan from the low end for a non-zero limb.  The first non-zero
       limb is simply negated (two's complement).  Any subsequent
       limbs are one's complemented.  Of course, we don't need to
       handle more limbs than there are limbs in the other, positive
       operand as the result for those limbs is going to become zero
       anyway.  */

    /* Scan for the least significant. non-zero OP2 limb, and zero the
       result meanwhile for those limb positions.  (We will surely
       find a non-zero limb, so we can write the loop with one
       termination condition only.)  */
    for (i = 0; op2_ptr[i] == 0; i++)
      res_ptr[i] = 0;
    op2_lim = i;

    op2_size = -op2_size;

    if (op1_size <= op2_size)
      {
	/* The ones-extended OP2 is >= than the zero-extended OP1.
	   RES_SIZE <= OP1_SIZE.  Find the exact size.  */
	for (i = op1_size - 1; i > op2_lim; i--)
	  if ((op1_ptr[i] & ~op2_ptr[i]) != 0)
	    break;
	res_size = i + 1;
	for (i = res_size - 1; i > op2_lim; i--)
	  res_ptr[i] = op1_ptr[i] & ~op2_ptr[i];
	res_ptr[op2_lim] = op1_ptr[op2_lim] & -op2_ptr[op2_lim];
	/* Yes, this *can* happen!  */
	MPN_NORMALIZE (res_ptr, res_size);
      }
    else
      {
	/* The ones-extended OP2 is < than the zero-extended OP1.
	   RES_SIZE == OP1_SIZE, since OP1 is normalized.  */
	res_size = op1_size;
	MPN_COPY (res_ptr + op2_size, op1_ptr + op2_size, op1_size - op2_size);
	for (i = op2_size - 1; i > op2_lim; i--)
	  res_ptr[i] = op1_ptr[i] & ~op2_ptr[i];
	res_ptr[op2_lim] = op1_ptr[op2_lim] & -op2_ptr[op2_lim];
      }

    res->_mp_size = res_size;
#else

    /* OP1 is positive and zero-extended,
       OP2 is negative and ones-extended.
       The result will be positive.
       OP1 & -OP2 = OP1 & ~(OP2 - 1).  */

    mp_ptr opx;

    op2_size = -op2_size;
    opx = (mp_ptr) TMP_ALLOC (op2_size * BYTES_PER_MP_LIMB);
    mpn_sub_1 (opx, op2_ptr, op2_size, (mp_limb_t) 1);
    op2_ptr = opx;

    if (op1_size > op2_size)
      {
	/* The result has the same size as OP1, since OP1 is normalized
	   and longer than the ones-extended OP2.  */
	res_size = op1_size;

	/* Handle allocation, now then we know exactly how much space is
	   needed for the result.  */
	if (res->_mp_alloc < res_size)
	  {
	    _mpz_realloc (res, res_size);
	    res_ptr = res->_mp_d;
	    op1_ptr = op1->_mp_d;
	    /* Don't re-read OP2_PTR.  It points to temporary space--never
	       to the space RES->_mp_D used to point to before reallocation.  */
	  }

	MPN_COPY (res_ptr + op2_size, op1_ptr + op2_size,
		  res_size - op2_size);
	for (i = op2_size - 1; i >= 0; i--)
	  res_ptr[i] = op1_ptr[i] & ~op2_ptr[i];

	res->_mp_size = res_size;
      }
    else
      {
	/* Find out the exact result size.  Ignore the high limbs of OP2,
	   OP1 is zero-extended and would make the result zero.  */
	for (i = op1_size - 1; i >= 0; i--)
	  if ((op1_ptr[i] & ~op2_ptr[i]) != 0)
	    break;
	res_size = i + 1;

	/* Handle allocation, now then we know exactly how much space is
	   needed for the result.  */
	if (res->_mp_alloc < res_size)
	  {
	    _mpz_realloc (res, res_size);
	    res_ptr = res->_mp_d;
	    op1_ptr = op1->_mp_d;
	    /* Don't re-read OP2_PTR.  It points to temporary space--never
	       to the space RES->_mp_D used to point to before reallocation.  */
	  }

	for (i = res_size - 1; i >= 0; i--)
	  res_ptr[i] = op1_ptr[i] & ~op2_ptr[i];

	res->_mp_size = res_size;
      }
#endif
  }
  TMP_FREE (marker);
}
