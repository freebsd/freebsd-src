/* mpz_mdivmod -- Mathematical DIVision and MODulo, i.e. division that rounds
   the quotient towards -infinity, and with the remainder non-negative.

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

void
#ifdef __STDC__
mpz_mdivmod (MP_INT *quot, MP_INT *rem,
	     const MP_INT *dividend, const MP_INT *divisor)
#else
mpz_mdivmod (quot, rem, dividend, divisor)
     MP_INT *quot;
     MP_INT *rem;
     const MP_INT *dividend;
     const MP_INT *divisor;
#endif
{
  if ((dividend->size ^ divisor->size) >= 0)
    {
      /* When the dividend and the divisor has same sign, this function
	 gives same result as mpz_divmod.  */
      mpz_divmod (quot, rem, dividend, divisor);
    }
  else
    {
      MP_INT temp_divisor;	/* N.B.: lives until function returns! */

      /* We need the original value of the divisor after the quotient and
	 remainder have been preliminary calculated.  We have to copy it to
	 temporary space if it's the same variable as either QUOT or REM.  */
      if (quot == divisor || rem == divisor)
	{
	  MPZ_TMP_INIT (&temp_divisor, ABS (divisor->size));
	  mpz_set (&temp_divisor, divisor);
	  divisor = &temp_divisor;
	}

      mpz_divmod (quot, rem, dividend, divisor);
      if (rem->size != 0)
	{
	  mpz_sub_ui (quot, quot, 1);
	  mpz_add (rem, rem, divisor);
	}
    }
}
