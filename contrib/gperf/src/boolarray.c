/* Fast lookup table abstraction implemented as a Guilmette Array
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

#include <stdio.h>
#include "boolarray.h"
#include "options.h"

/* Locally visible BOOL_ARRAY object. */

static BOOL_ARRAY bool_array;

/* Prints out debugging diagnostics. */

void
bool_array_destroy ()
{
  if (OPTION_ENABLED (option, DEBUG))
    fprintf (stderr, "\ndumping boolean array information\niteration number = %d\nend of array dump\n",
             bool_array.iteration_number);
  free ((char *) bool_array.storage_array);
}

void
bool_array_init (size)
     int size;
{
	STORAGE_TYPE *xmalloc ();
  bool_array.iteration_number = 1;
  bool_array.size = size;
  bool_array.storage_array = xmalloc (size * sizeof *bool_array.storage_array);
  bzero (bool_array.storage_array, size * sizeof *bool_array.storage_array);
  if (OPTION_ENABLED (option, DEBUG))
    fprintf (stderr, "\nbool array size = %d, total bytes = %d\n",
             bool_array.size, bool_array.size * sizeof *bool_array.storage_array);
}

bool 
lookup (index)
     int index;
{
  if (bool_array.storage_array[index] == bool_array.iteration_number)
    return 1;
  else
    {
      bool_array.storage_array[index] = bool_array.iteration_number;
      return 0;
    }
}

/* Simple enough to reset, eh?! */

void 
bool_array_reset ()  
{
  /* If we wrap around it's time to zero things out again! */
            
  
  if (++bool_array.iteration_number == 0)
    {
      if (OPTION_ENABLED (option, DEBUG))
        {
          fprintf (stderr, "(re-initializing bool_array)...");
          fflush (stderr);
        }
      bool_array.iteration_number = 1;
      bzero (bool_array.storage_array, bool_array.size * sizeof *bool_array.storage_array);
      if (OPTION_ENABLED (option, DEBUG))
        {
          fprintf (stderr, "done\n");
          fflush (stderr);
        }
    }
}
