/* mpf_out_str (stream, base, n_digits, op) -- Print N_DIGITS digits from
   the float OP to STREAM in base BASE.  Return the number of characters
   written, or 0 if an error occurred.

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

#include <stdio.h>
#include "gmp.h"
#include "gmp-impl.h"

size_t
#if __STDC__
mpf_out_str (FILE *stream, int base, size_t n_digits, mpf_srcptr op)
#else
mpf_out_str (stream, base, n_digits, op)
     FILE *stream;
     int base;
     size_t n_digits;
     mpf_srcptr op;
#endif
{
  char *str;
  mp_exp_t exp;
  size_t written;
  TMP_DECL (marker);

  TMP_MARK (marker);

  if (base == 0)
    base = 10;
  if (n_digits == 0)
    n_digits = (((op->_mp_prec - 1) * BITS_PER_MP_LIMB)
		* __mp_bases[base].chars_per_bit_exactly);

  if (stream == 0)
    stream = stdout;

  str = (char *) TMP_ALLOC (n_digits + 2); /* extra for minus sign and \0 */

  mpf_get_str (str, &exp, base, n_digits, op);
  n_digits = strlen (str);

  written = 0;

  /* Write sign */
  if (str[0] == '-')
    {
      str++;
      fputc ('-', stream);
      written = 1;
    }

  fwrite ("0.", 1, 2, stream);
  written += 2;

  /* Write mantissa */
  {
    size_t fwret;
    fwret = fwrite (str, 1, n_digits, stream);
    written += fwret;
  }

  /* Write exponent */
  {
    int fpret;
    fpret = fprintf (stream, (base <= 10 ? "e%ld" : "@%ld"), exp);
    written += fpret;
  }

  TMP_FREE (marker);
  return ferror (stream) ? 0 : written;
}
