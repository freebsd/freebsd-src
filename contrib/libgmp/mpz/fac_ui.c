/* mpz_fac_ui(result, n) -- Set RESULT to N!.

Copyright (C) 1991, 1993, 1994, 1995 Free Software Foundation, Inc.

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

#ifdef DBG
#include <stdio.h>
#endif

#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

void
#if __STDC__
mpz_fac_ui (mpz_ptr result, unsigned long int n)
#else
mpz_fac_ui (result, n)
     mpz_ptr result;
     unsigned long int n;
#endif
{
#if SIMPLE_FAC

  /* Be silly.  Just multiply the numbers in ascending order.  O(n**2).  */

  unsigned long int k;

  mpz_set_ui (result, 1L);

  for (k = 2; k <= n; k++)
    mpz_mul_ui (result, result, k);
#else

  /* Be smarter.  Multiply groups of numbers in ascending order until the
     product doesn't fit in a limb.  Multiply these partial product in a
     balanced binary tree fashion, to make the operand have as equal sizes
     as possible.  When the operands have about the same size, mpn_mul
     becomes faster.  */

  unsigned long int p, k;
  mp_limb_t p1, p0;

  /* Stack of partial products, used to make the computation balanced
     (i.e. make the sizes of the multiplication operands equal).  The
     topmost position of MP_STACK will contain a one-limb partial product,
     the second topmost will contain a two-limb partial product, and so
     on.  MP_STACK[0] will contain a partial product with 2**t limbs.
     To compute n! MP_STACK needs to be less than
     log(n)**2/log(BITS_PER_MP_LIMB), so 30 is surely enough.  */
#define MP_STACK_SIZE 30
  mpz_t mp_stack[MP_STACK_SIZE];

  /* TOP is an index into MP_STACK, giving the topmost element.
     TOP_LIMIT_SO_FAR is the largets value it has taken so far.  */
  int top, top_limit_so_far;

  /* Count of the total number of limbs put on MP_STACK so far.  This
     variable plays an essential role in making the compututation balanced.
     See below.  */
  unsigned int tree_cnt;

  top = top_limit_so_far = -1;
  tree_cnt = 0;
  p = 1;
  for (k = 2; k <= n; k++)
    {
      /* Multiply the partial product in P with K.  */
      umul_ppmm (p1, p0, (mp_limb_t) p, (mp_limb_t) k);

      /* Did we get overflow into the high limb, i.e. is the partial
	 product now more than one limb?  */
      if (p1 != 0)
	{
	  tree_cnt++;

	  if (tree_cnt % 2 == 0)
	    {
	      mp_size_t i;

	      /* TREE_CNT is even (i.e. we have generated an even number of
		 one-limb partial products), which means that we have a
		 single-limb product on the top of MP_STACK.  */

	      mpz_mul_ui (mp_stack[top], mp_stack[top], p);

	      /* If TREE_CNT is divisable by 4, 8,..., we have two
		 similar-sized partial products with 2, 4,... limbs at
		 the topmost two positions of MP_STACK.  Multiply them
		 to form a new partial product with 4, 8,... limbs.  */
	      for (i = 4; (tree_cnt & (i - 1)) == 0; i <<= 1)
		{
		  mpz_mul (mp_stack[top - 1],
			   mp_stack[top], mp_stack[top - 1]);
		  top--;
		}
	    }
	  else
	    {
	      /* Put the single-limb partial product in P on the stack.
		 (The next time we get a single-limb product, we will
		 multiply the two together.)  */
	      top++;
	      if (top > top_limit_so_far)
		{
		  if (top > MP_STACK_SIZE)
		    abort();
		  /* The stack is now bigger than ever, initialize the top
		     element.  */
		  mpz_init_set_ui (mp_stack[top], p);
		  top_limit_so_far++;
		}
	      else
		mpz_set_ui (mp_stack[top], p);
	    }

	  /* We ignored the last result from umul_ppmm.  Put K in P as the
	     first component of the next single-limb partial product.  */
	  p = k;
	}
      else
	/* We didn't get overflow in umul_ppmm.  Put p0 in P and try
	   with one more value of K.  */
	p = p0;			/* bogus if long != mp_limb_t */
    }

  /* We have partial products in mp_stack[0..top], in descending order.
     We also have a small partial product in p.
     Their product is the final result.  */
  if (top < 0)
    mpz_set_ui (result, p);
  else
    mpz_mul_ui (result, mp_stack[top--], p);
  while (top >= 0)
    mpz_mul (result, result, mp_stack[top--]);

  /* Free the storage allocated for MP_STACK.  */
  for (top = top_limit_so_far; top >= 0; top--)
    mpz_clear (mp_stack[top]);
#endif
}
