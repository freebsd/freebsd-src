/* min(MINT) -- Do decimal input from standard input and store result in
   MINT.

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

#include <stdio.h>
#include <ctype.h>
#include "mp.h"
#include "gmp.h"
#include "gmp-impl.h"

void
#ifdef __STDC__
min (MINT *x)
#else
min (x)
     MINT *x;
#endif
{
  char *str;
  size_t str_size;
  size_t i;
  int c;

  str_size = 100;
  str = (char *) (*_mp_allocate_func) (str_size);

  for (i = 0; ; i++)
    {
      if (i >= str_size)
	{
	  size_t old_str_size = str_size;
	  str_size = str_size * 3 / 2;
	  str = (char *) (*_mp_reallocate_func) (str, old_str_size, str_size);
	}
      c = getc (stdin);
      if (!(isdigit(c) || c == ' ' || c == '\t'))
	break;
      str[i] = c;
    }

  ungetc (c, stdin);

  str[i] = 0;
  _mpz_set_str (x, str, 10);

  (*_mp_free_func) (str, str_size);
}
