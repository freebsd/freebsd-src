/* mpz_get_str (string, base, mp_src) -- Convert the multiple precision
   number MP_SRC to a string STRING of base BASE.  If STRING is NULL
   allocate space for the result.  In any case, return a pointer to the
   result.  If STRING is not NULL, the caller must ensure enough space is
   available to store the result.

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

char *
#ifdef __STDC__
mpz_get_str (char *str, int base, const MP_INT *m)
#else
mpz_get_str (str, base, m)
     char *str;
     int base;
     const MP_INT *m;
#endif
{
  return _mpz_get_str (str, base, m);
}
