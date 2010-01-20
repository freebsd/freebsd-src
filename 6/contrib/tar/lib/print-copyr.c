/* Print a copyright notice suitable for the current locale.
   Copyright (C) 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Paul Eggert.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "unicodeio.h"
#include "print-copyr.h"

#include <stdio.h>

#define COPYRIGHT_SIGN 0x00A9

/* Print "(C)".  */

static int
print_parenthesized_c (unsigned int code, void *callback_arg)
{
  FILE *stream = callback_arg;
  return fputs ("(C)", stream);
}

/* Print "Copyright (C) " followed by NOTICE and then a newline,
   transliterating "(C)" to an actual copyright sign (C-in-a-circle)
   if possible.  */

void
print_copyright (char const *notice)
{
  fputs ("Copyright ", stdout);
  unicode_to_mb (COPYRIGHT_SIGN, print_unicode_success, print_parenthesized_c,
		 stdout);
  fputc (' ', stdout);
  puts (notice);
}
