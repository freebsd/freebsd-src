/* Simple lookup table abstraction implemented as a Guilmette Array.

   Copyright (C) 1989 Free Software Foundation, Inc.
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
along with GNU GPERF; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Define and implement a simple boolean array abstraction,
   uses a Guilmette array implementation to save on initialization time. */ 

#ifndef _boolarray_h
#define _boolarray_h
#include "prototype.h"

#ifdef LO_CAL
/* If we are on a memory diet then we'll only make these use a limited
   amount of storage space. */
typedef unsigned short STORAGE_TYPE;
#else
typedef int STORAGE_TYPE;
#endif
typedef struct bool_array 
{
  STORAGE_TYPE *storage_array;    /* Initialization of the index space. */
  STORAGE_TYPE  iteration_number; /* Keep track of the current iteration. */
  int  size;                      /* Size of the entire array (dynamically initialized). */
} BOOL_ARRAY;

extern void bool_array_init P ((int size));
extern void bool_array_destroy P ((void));
extern bool lookup P ((int hash_value));
extern void bool_array_reset P ((void));

#endif /* _boolarray_h */
