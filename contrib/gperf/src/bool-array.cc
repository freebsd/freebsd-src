/* Fast lookup table abstraction implemented as an Iteration Number Array
   Copyright (C) 1989-1998 Free Software Foundation, Inc.
   written by Douglas C. Schmidt (schmidt@ics.uci.edu)

This file is part of GNU GPERF.

GNU GPERF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU GPERF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU GPERF; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111, USA.  */

#include "bool-array.h"

#include <stdio.h>
#include <string.h>
#include "options.h"
#include "trace.h"

STORAGE_TYPE * Bool_Array::storage_array;
STORAGE_TYPE Bool_Array::iteration_number;
unsigned int Bool_Array::size;

/* Prints out debugging diagnostics. */

Bool_Array::~Bool_Array (void)
{
  T (Trace t ("Bool_Array::~Bool_Array");)
  if (option[DEBUG])
    fprintf (stderr, "\ndumping boolean array information\n"
             "size = %d\niteration number = %d\nend of array dump\n",
             size, iteration_number);
}

#ifndef __OPTIMIZE__

#define INLINE /* not inline */
#include "bool-array.icc"
#undef INLINE

#endif /* not defined __OPTIMIZE__ */
