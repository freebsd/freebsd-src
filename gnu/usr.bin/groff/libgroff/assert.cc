/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

extern "C" {
#include <stdio.h>
#include <stdlib.h>
}
#include "assert.h"

extern const char *program_name;

#ifdef __GNUG__
volatile
#endif
void assertion_failed(int lineno, const char *filename)
{
  if (program_name != 0)
    fprintf(stderr, "%s: ", program_name);
  fprintf(stderr, "Failed assertion at line %d, file `%s'.\n",
	  lineno, filename);
  fflush(stderr);
  abort();
}
