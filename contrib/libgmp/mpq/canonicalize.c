/* mpq_canonicalize(op) -- Remove common factors of the denominator and
   numerator in OP.

Copyright (C) 1991, 1994, 1995, 1996 Free Software Foundation, Inc.

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
mpq_canonicalize (MP_RAT *op)
#else
mpq_canonicalize (op)
     MP_RAT *op;
#endif
{
  mpz_t gcd;
  TMP_DECL (marker);

  TMP_MARK (marker);

  /* ??? Dunno if the 1+ is needed.  */
  MPZ_TMP_INIT (gcd, 1 + MAX (ABS (op->_mp_num._mp_size),
			      ABS (op->_mp_den._mp_size)));

  mpz_gcd (gcd, &(op->_mp_num), &(op->_mp_den));
  mpz_divexact (&(op->_mp_num), &(op->_mp_num), gcd);
  mpz_divexact (&(op->_mp_den), &(op->_mp_den), gcd);

  if (op->_mp_den._mp_size < 0)
    {
      op->_mp_num._mp_size = -op->_mp_num._mp_size;
      op->_mp_den._mp_size = -op->_mp_den._mp_size;
    }
  TMP_FREE (marker);
}
