/* mpz_and -- Logical and.

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

#define min(l,o) ((l) < (o) ? (l) : (o))
#define max(h,i) ((h) > (i) ? (h) : (i))

void
#ifdef __STDC__
mpz_and (MP_INT *res, const MP_INT *op1, const MP_INT *op2)
#else
mpz_and (res, op1, op2)
     MP_INT *res;
     const MP_INT *op1;
     const MP_INT *op2;
#endif
{
  mp_srcptr op1_ptr, op2_ptr;
  mp_size op1_size, op2_size;
  mp_ptr res_ptr;
  mp_size res_size;
  mp_size i;

  op1_size = op1->size;
  op2_size = op2->size;

  op1_ptr = op1->d;
  op2_ptr = op2->d;
  res_ptr = res->d;

  if (op1_size >= 0)
    {
      if (op2_size >= 0)
	{
	  res_size = min (op1_size, op2_size);
	  /* First loop finds the size of the result.  */
	  for (i = res_size - 1; i >= 0; i--)
	    if ((op1_ptr[i] & op2_ptr[i]) != 0)
	      break;
	  res_size = i + 1;

	  /* Handle allocation, now when we know exactly how much space is
	     needed for the result.  */
	  if (res->alloc < res_size)
	    {
	      _mpz_realloc (res, res_size);
	      op1_ptr = op1->d;
	      op2_ptr = op2->d;
	      res_ptr = res->d;
	    }

	  /* Second loop computes the real result.  */
	  for (i = res_size - 1; i >= 0; i--)
	    res_ptr[i] = op1_ptr[i] & op2_ptr[i];

	  res->size = res_size;
	  return;
	}
      else /* op2_size < 0 */
	/* Fall through to the code at the end of the function.  */
	;
    }
  else
    {
      if (op2_size < 0)
	{
	  mp_ptr opx;
	  mp_limb cy;
	  mp_limb one = 1;
	  mp_size res_alloc;

	  /* Both operands are negative, so will be the result.
	     -((-OP1) & (-OP2)) = -(~(OP1 - 1) & ~(OP2 - 1)) =
	     = ~(~(OP1 - 1) & ~(OP2 - 1)) + 1 =
	     = ((OP1 - 1) | (OP2 - 1)) + 1      */

	  op1_size = -op1_size;
	  op2_size = -op2_size;

	  res_alloc = 1 + max (op1_size, op2_size);

	  opx = (mp_ptr) alloca (op1_size * BYTES_PER_MP_LIMB);
	  op1_size += mpn_sub (opx, op1_ptr, op1_size, &one, 1);
	  op1_ptr = opx;

	  opx = (mp_ptr) alloca (op2_size * BYTES_PER_MP_LIMB);
	  op2_size += mpn_sub (opx, op2_ptr, op2_size, &one, 1);
	  op2_ptr = opx;

	  if (res->alloc < res_alloc)
	    {
	      _mpz_realloc (res, res_alloc);
	      res_ptr = res->d;
	      /* Don't re-read OP1_PTR and OP2_PTR.  They point to
		 temporary space--never to the space RES->D used
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

	  if (res_size != 0)
	    {
	      cy = mpn_add (res_ptr, res_ptr, res_size, &one, 1);
	      if (cy)
		{
		  res_ptr[res_size] = cy;
		  res_size++;
		}
	    }
	  else
	    {
	      res_ptr[0] = 1;
	      res_size = 1;
	    }

	  res->size = -res_size;
	  return;
	}
      else
	{
	  /* We should compute -OP1 & OP2.  Swap OP1 and OP2 and fall
	     through to the code that handles OP1 & -OP2.  */
	  {const MP_INT *t = op1; op1 = op2; op2 = t;}
	  {mp_srcptr t = op1_ptr; op1_ptr = op2_ptr; op2_ptr = t;}
	  {mp_size t = op1_size; op1_size = op2_size; op2_size = t;}
	}

    }

  {
#if 0
    mp_size op2_lim;

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
      }
    else
      {
	/* The ones-extended OP2 is < than the zero-extended OP1.
	   RES_SIZE == OP1_SIZE, since OP1 is normalized.  */
	res_size = op1_size;
      }
#endif

    /* OP1 is positive and zero-extended,
       OP2 is negative and ones-extended.
       The result will be positive.
       OP1 & -OP2 = OP1 & ~(OP2 - 1).  */

    mp_ptr opx;
    const mp_limb one = 1;

    op2_size = -op2_size;
    opx = (mp_ptr) alloca (op2_size * BYTES_PER_MP_LIMB);
    op2_size += mpn_sub (opx, op2_ptr, op2_size, &one, 1);
    op2_ptr = opx;

    if (op1_size > op2_size)
      {
	/* The result has the same size as OP1, since OP1 is normalized
	   and longer than the ones-extended OP2.  */
	res_size = op1_size;

	/* Handle allocation, now when we know exactly how much space is
	   needed for the result.  */
	if (res->alloc < res_size)
	  {
	    _mpz_realloc (res, res_size);
	    res_ptr = res->d;
	    op1_ptr = op1->d;
	    /* Don't re-read OP2_PTR.  It points to temporary space--never
	       to the space RES->D used to point to before reallocation.  */
	  }

	MPN_COPY (res_ptr + op2_size, op1_ptr + op2_size,
		  res_size - op2_size);
	for (i = op2_size - 1; i >= 0; i--)
	  res_ptr[i] = op1_ptr[i] & ~op2_ptr[i];

	res->size = res_size;
      }
    else
      {
	/* Find out the exact result size.  Ignore the high limbs of OP2,
	   OP1 is zero-extended and would make the result zero.  */
	for (i = op1_size - 1; i >= 0; i--)
	  if ((op1_ptr[i] & ~op2_ptr[i]) != 0)
	    break;
	res_size = i + 1;

	/* Handle allocation, now when we know exactly how much space is
	   needed for the result.  */
	if (res->alloc < res_size)
	  {
	    _mpz_realloc (res, res_size);
	    res_ptr = res->d;
	    op1_ptr = op1->d;
	    /* Don't re-read OP2_PTR.  It points to temporary space--never
	       to the space RES->D used to point to before reallocation.  */
	  }

	for (i = res_size - 1; i >= 0; i--)
	  res_ptr[i] = op1_ptr[i] & ~op2_ptr[i];

	res->size = res_size;
      }
  }
}
