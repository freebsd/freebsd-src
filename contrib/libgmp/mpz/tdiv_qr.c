/* mpz_tdiv_qr(quot,rem,dividend,divisor) -- Set QUOT to DIVIDEND/DIVISOR,
   and REM to DIVIDEND mod DIVISOR.

Copyright (C) 1991, 1993, 1994 Free Software Foundation, Inc.

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
#include "longlong.h"

void
#if __STDC__
mpz_tdiv_qr (mpz_ptr quot, mpz_ptr rem, mpz_srcptr num, mpz_srcptr den)
#else
mpz_tdiv_qr (quot, rem, num, den)
     mpz_ptr quot;
     mpz_ptr rem;
     mpz_srcptr num;
     mpz_srcptr den;
#endif

#define COMPUTE_QUOTIENT
#include "dmincl.c"
