/* mpf_inp_str(dest_float, stream, base) -- Input a number in base
   BASE from stdio stream STREAM and store the result in DEST_FLOAT.

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
#include <ctype.h>
#include "gmp.h"
#include "gmp-impl.h"

size_t
#if __STDC__
mpf_inp_str (mpf_ptr rop, FILE *stream, int base)
#else
mpf_inp_str (rop, stream, base)
     mpf_ptr rop;
     FILE *stream;
     int base;
#endif
{
  char *str;
  size_t alloc_size, str_size;
  int c;
  size_t retval;
  size_t nread;

  if (stream == 0)
    stream = stdin;

  alloc_size = 100;
  str = (char *) (*_mp_allocate_func) (alloc_size);
  str_size = 0;
  nread = 0;

  /* Skip whitespace.  */
  do
    {
      c = getc (stream);
      nread++;
    }
  while (isspace (c));

  for (;;)
    {
      if (str_size >= alloc_size)
	{
	  size_t old_alloc_size = alloc_size;
	  alloc_size = alloc_size * 3 / 2;
	  str = (char *) (*_mp_reallocate_func) (str, old_alloc_size, alloc_size);
	}
      if (c == EOF || isspace (c))
	break;
      str[str_size++] = c;
      c = getc (stream);
    }
  ungetc (c, stream);

  if (str_size >= alloc_size)
    {
      size_t old_alloc_size = alloc_size;
      alloc_size = alloc_size * 3 / 2;
      str = (char *) (*_mp_reallocate_func) (str, old_alloc_size, alloc_size);
    }
  str[str_size] = 0;

  retval = mpf_set_str (rop, str, base);
  if (retval == -1)
    return 0;			/* error */

  (*_mp_free_func) (str, alloc_size);
  return str_size + nread;
}
