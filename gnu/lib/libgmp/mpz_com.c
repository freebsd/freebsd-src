/* mpz_com(MP_INT *dst, MP_INT *src) -- Assign the bit-complemented value of
   SRC to DST.

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

void
#ifdef __STDC__
mpz_com (MP_INT *dst, const MP_INT *src)
#else
mpz_com (dst, src)
     MP_INT *dst;
     const MP_INT *src;
#endif
{
  mp_size size = src->size;
  mp_srcptr src_ptr;
  mp_ptr dst_ptr;

  if (size >= 0)
    {
      /* As with infinite precision: one's complement, two's complement.
	 But this can be simplified using the identity -x = ~x + 1.
	 So we're going to compute (~~x) + 1 = x + 1!  */

      if (dst->alloc < size + 1)
	_mpz_realloc (dst, size + 1);

      src_ptr = src->d;
      dst_ptr = dst->d;

      if (size == 0)
	{
	  /* Special case, as mpn_add wants the first arg's size >= the
	     second arg's size.  */
	  dst_ptr[0] = 1;
	  dst->size = -1;
	  return;
	}

      {
	mp_limb one = 1;
	int cy;

	cy = mpn_add (dst_ptr, src_ptr, size, &one, 1);
	if (cy)
	  {
	    dst_ptr[size] = cy;
	    size++;
	  }
      }

      /* Store a negative size, to indicate ones-extension.  */
      dst->size = -size;
    }
  else
    {
      /* As with infinite precision: two's complement, then one's complement.
	 But that can be simplified using the identity -x = ~(x - 1).
	 So we're going to compute ~~(x - 1) = x - 1!  */
      size = -size;

      if (dst->alloc < size)
	_mpz_realloc (dst, size);

      src_ptr = src->d;
      dst_ptr = dst->d;

      {
	mp_limb one = 1;

	size += mpn_sub (dst_ptr, src_ptr, size, &one, 1);
      }

      /* Store a positive size, to indicate zero-extension.  */
      dst->size = size;
    }
}
