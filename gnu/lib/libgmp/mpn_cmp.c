/* mpn_cmp -- Compare two low-level natural-number integers.

Copyright (C) 1991, 1992 Free Software Foundation, Inc.

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

/* Compare OP1_PTR/OP1_SIZE with OP2_PTR/OP2_SIZE.
   There are no restrictions on the relative sizes of
   the two arguments.
   Return 1 if OP1 > OP2, 0 if they are equal, and -1 if OP1 < OP2.  */

int
#ifdef __STDC__
mpn_cmp (mp_srcptr op1_ptr, mp_srcptr op2_ptr, mp_size size)
#else
mpn_cmp (op1_ptr, op2_ptr, size)
     mp_srcptr op1_ptr;
     mp_srcptr op2_ptr;
     mp_size size;
#endif
{
  mp_size i;
  mp_limb op1_word, op2_word;

  for (i = size - 1; i >= 0; i--)
    {
      op1_word = op1_ptr[i];
      op2_word = op2_ptr[i];
      if (op1_word != op2_word)
	goto diff;
    }
  return 0;
 diff:
  return (op1_word > op2_word) ? 1 : -1;
}
