/* mpf_dump -- Dump a float to stdout.

Copyright (C) 1993, 1994, 1995 Free Software Foundation, Inc.

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

void
#if __STDC__
mpf_dump (mpf_srcptr u)
#else
mpf_dump (u)
     mpf_srcptr u;
#endif
{
  mp_exp_t exp;
  char *str;

  str = mpf_get_str (0, &exp, 10, 0, u);
  if (str[0] == '-')
    printf ("-0.%se%ld\n", str + 1, exp);
  else
    printf ("0.%se%ld\n", str, exp);
  (*_mp_free_func) (str, 0);/* ??? broken alloc interface, pass what size ??? */
}
