/* mpz_sqrt(root, u) --  Set ROOT to floor(sqrt(U)).

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
mpz_sqrt (mpz_ptr root, mpz_srcptr op)
#else
mpz_sqrt (root, op)
     mpz_ptr root;
     mpz_srcptr op;
#endif
{
  mp_size_t op_size, root_size;
  mp_ptr root_ptr, op_ptr;
  mp_ptr free_me = NULL;
  mp_size_t free_me_size;
  TMP_DECL (marker);

  TMP_MARK (marker);
  op_size = op->_mp_size;
  if (op_size < 0)
    op_size = 1 / (op_size > 0); /* Divide by zero for negative OP.  */

  /* The size of the root is accurate after this simple calculation.  */
  root_size = (op_size + 1) / 2;

  root_ptr = root->_mp_d;
  op_ptr = op->_mp_d;

  if (root->_mp_alloc < root_size)
    {
      if (root_ptr == op_ptr)
	{
	  free_me = root_ptr;
	  free_me_size = root->_mp_alloc;
	}
      else
	(*_mp_free_func) (root_ptr, root->_mp_alloc * BYTES_PER_MP_LIMB);

      root->_mp_alloc = root_size;
      root_ptr = (mp_ptr) (*_mp_allocate_func) (root_size * BYTES_PER_MP_LIMB);
      root->_mp_d = root_ptr;
    }
  else
    {
      /* Make OP not overlap with ROOT.  */
      if (root_ptr == op_ptr)
	{
	  /* ROOT and OP are identical.  Allocate temporary space for OP.  */
	  op_ptr = (mp_ptr) TMP_ALLOC (op_size * BYTES_PER_MP_LIMB);
	  /* Copy to the temporary space.  Hack: Avoid temporary variable
	     by using ROOT_PTR.  */
	  MPN_COPY (op_ptr, root_ptr, op_size);
	}
    }

  mpn_sqrtrem (root_ptr, NULL, op_ptr, op_size);

  root->_mp_size = root_size;

  if (free_me != NULL)
    (*_mp_free_func) (free_me, free_me_size * BYTES_PER_MP_LIMB);
  TMP_FREE (marker);
}
