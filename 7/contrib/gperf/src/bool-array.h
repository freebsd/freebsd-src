/* This may look like C code, but it is really -*- C++ -*- */

/* Simple lookup table abstraction implemented as an Iteration Number Array.

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
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/* Define and implement a simple boolean array abstraction,
   uses an Iteration Numbering implementation to save on initialization time. */

#ifndef bool_array_h
#define bool_array_h 1

#include "trace.h"

#ifdef LO_CAL
/* If we are on a memory diet then we'll only make these use a limited
   amount of storage space. */
typedef unsigned short STORAGE_TYPE;
#else
typedef unsigned int STORAGE_TYPE;
#endif

class Bool_Array
{
private:
  static STORAGE_TYPE *storage_array;    /* Initialization of the index space. */
  static STORAGE_TYPE  iteration_number; /* Keep track of the current iteration. */
  static unsigned int  size;             /* Keep track of array size. */

public:
       Bool_Array (void);
      ~Bool_Array (void);
  static void init (STORAGE_TYPE *buffer, unsigned int s);
  static int  find (int hash_value);
  static void reset (void);
};

#ifdef __OPTIMIZE__  /* efficiency hack! */

#include <stdio.h>
#include <string.h>
#include "options.h"
#define INLINE inline
#include "bool-array.icc"
#undef INLINE

#endif

#endif
