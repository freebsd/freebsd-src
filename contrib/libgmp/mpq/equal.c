/* mpq_equal(u,v) -- Compare U, V.  Return non-zero if they are equal, zero
   if they are non-equal.

Copyright (C) 1996 Free Software Foundation, Inc.

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

int
#if __STDC__
mpq_equal (mpq_srcptr op1, mpq_srcptr op2)
#else
mpq_equal (op1, op2)
     mpq_srcptr op1;
     mpq_srcptr op2;
#endif
{
  mp_size_t num1_size = op1->_mp_num._mp_size;
  mp_size_t den1_size = op1->_mp_den._mp_size;
  mp_size_t num2_size = op2->_mp_num._mp_size;
  mp_size_t den2_size = op2->_mp_den._mp_size;

  return (num1_size == num2_size && den1_size == den2_size
	  && mpn_cmp (op1->_mp_num._mp_d, op2->_mp_num._mp_d, num1_size) == 0
	  && mpn_cmp (op1->_mp_den._mp_d, op2->_mp_den._mp_d, den1_size) == 0);
}
