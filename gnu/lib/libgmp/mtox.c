/* mtox -- Convert OPERAND to hexadecimal and return a malloc'ed string
   with the result of the conversion.

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

#include "mp.h"
#include "gmp.h"
#include "gmp-impl.h"

char *
#ifdef __STDC__
mtox (const MINT *operand)
#else
mtox (operand)
     const MINT *operand;
#endif
{
  /* Call MP_GET_STR with a NULL pointer as string argument, so that it
     allocates space for the result.  */
  return _mpz_get_str ((char *) 0, 16, operand);
}
