/* mpz_mmod -- Mathematical MODulo, i.e. with the remainder
   non-negative.

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
mpz_mmod (MP_INT *rem,
	     const MP_INT *dividend, const MP_INT *divisor)
#else
mpz_mmod (rem, dividend, divisor)
     MP_INT *rem;
     const MP_INT *dividend;
     const MP_INT *divisor;
#endif
{
  if ((dividend->size ^ divisor->size) >= 0)
    {
      /* When the dividend and the divisor has same sign, this function
	 gives same result as mpz_mod.  */
      mpz_mod (rem, dividend, divisor);
    }
  else
    {
      MP_INT temp_divisor;	/* N.B.: lives until function returns! */
 
      /* We need the original value of the divisor after the remainder has
	 been preliminary calculated.  We have to copy it to temporary
	 space if it's the same variable as REM.  */
      if (rem == divisor)
	{
	  MPZ_TMP_INIT (&temp_divisor, ABS (divisor->size));
	  mpz_set (&temp_divisor, divisor);
	  divisor = &temp_divisor;
	}

      mpz_mod (rem, dividend, divisor);
      if (rem->size != 0)
	mpz_add (rem, rem, divisor);
    }
}
