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

unsigned long int
#ifdef __STDC__
mpz_mmod_ui (MP_INT *rem,
	     const MP_INT *dividend, unsigned long int divisor)
#else
mpz_mmod_ui (rem, dividend, divisor)
     MP_INT *rem;
     const MP_INT *dividend;
     unsigned long int divisor;
#endif
{
  MP_INT temp_rem;	/* N.B.: lives until function returns! */

  if (rem == NULL)
    {
      MPZ_TMP_INIT (&temp_rem, 1 + ABS (dividend->size));
      rem = &temp_rem;
    }

  mpz_mod_ui (rem, dividend, divisor);

  if (rem->size < 0)
    mpz_add_ui (rem, rem, divisor);

  if (rem->size == 0)
    return 0;
  return rem->d[0];
}
