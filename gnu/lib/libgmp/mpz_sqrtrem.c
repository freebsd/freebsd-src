/* mpz_sqrtrem(root,rem,x) -- Set ROOT to floor(sqrt(X)) and REM
   to the remainder, i.e. X - ROOT**2.

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

#ifndef BERKELEY_MP
void
#ifdef __STDC__
mpz_sqrtrem (MP_INT *root, MP_INT *rem, const MP_INT *op)
#else
mpz_sqrtrem (root, rem, op)
     MP_INT *root;
     MP_INT *rem;
     const MP_INT *op;
#endif
#else /* BERKELEY_MP */
void
#ifdef __STDC__
msqrt (const MP_INT *op, MP_INT *root, MP_INT *rem)
#else
msqrt (op, root, rem)
     const MP_INT *op;
     MP_INT *root;
     MP_INT *rem;
#endif
#endif /* BERKELEY_MP */
{
  mp_size op_size, root_size, rem_size;
  mp_ptr root_ptr, op_ptr;
  mp_ptr free_me = NULL;
  mp_size free_me_size;

  op_size = op->size;
  if (op_size < 0)
    op_size = 1 / (op_size > 0);	/* Divide by zero for negative OP.  */

  if (rem->alloc < op_size)
    _mpz_realloc (rem, op_size);

  /* The size of the root is accurate after this simple calculation.  */
  root_size = (op_size + 1) / 2;

  root_ptr = root->d;
  op_ptr = op->d;

  if (root->alloc < root_size)
    {
      if (root_ptr == op_ptr)
	{
	  free_me = root_ptr;
	  free_me_size = root->alloc;
	}
      else
	(*_mp_free_func) (root_ptr, root->alloc * BYTES_PER_MP_LIMB);

      root->alloc = root_size;
      root_ptr = (mp_ptr) (*_mp_allocate_func) (root_size * BYTES_PER_MP_LIMB);
      root->d = root_ptr;
    }
  else
    {
      /* Make OP not overlap with ROOT.  */
      if (root_ptr == op_ptr)
	{
	  /* ROOT and OP are identical.  Allocate temporary space for OP.  */
	  op_ptr = (mp_ptr) alloca (op_size * BYTES_PER_MP_LIMB);
	  /* Copy to the temporary space.  Hack: Avoid temporary variable
	   by using ROOT_PTR.  */
	  MPN_COPY (op_ptr, root_ptr, op_size);
	}
    }

  rem_size = mpn_sqrt (root_ptr, rem->d, op_ptr, op_size);

  root->size = root_size;

  /* Write remainder size last, to enable us to define this function to
     give only the square root remainder, if the user calles if with
     ROOT == REM.  */
  rem->size = rem_size;

  if (free_me != NULL)
    (*_mp_free_func) (free_me, free_me_size * BYTES_PER_MP_LIMB);

  alloca (0);
}
