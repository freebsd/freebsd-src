/* Hash table for checking keyword links.  Implemented using double hashing.
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
#include "hashtable.h"
#include "options.h"

#ifdef GATHER_STATISTICS
/* Find out how well our double hashing is working! */
static collisions = 0;
#endif

/* Locally visible hash table. */
static HASH_TABLE hash_table;

/* Basically the algorithm from the Dragon book. */

static unsigned
hash_pjw (str)
     char *str;
{
  char    *temp;
  unsigned g, h = 0;
   
  for (temp = str; *temp; temp++) 
    {
      h = (h << 4) + (*temp * 13);
      if (g = h & 0xf0000000) 
        {
          h ^= (g >> 24);
          h ^= g;
        }
    }

  return h;
}

/* The size of the hash table is always the smallest power of 2 >= the size
   indicated by the user.  This allows several optimizations, including
   the use of double hashing and elimination of the mod instruction.
   Note that the size had better be larger than the number of items
   in the hash table, else there's trouble!!!  Note that the memory
   for the hash table is allocated *outside* the intialization routine.
   This compromises information hiding somewhat, but greatly reduces
   memory fragmentation, since we can now use alloca! */

void
hash_table_init (table, s)
     LIST_NODE **table;
     int s;
{
  hash_table.size  = s;
  hash_table.table = table;
  bzero ((char *) hash_table.table, hash_table.size * sizeof *hash_table.table);
}

/* Frees the dynamically allocated table.  Note that since we don't
   really need this space anymore, and since it is potentially quite
   big it is best to return it when we are done. */

void
hash_table_destroy ()
{ 
  if (OPTION_ENABLED (option, DEBUG))
    {
      int i;

      fprintf (stderr, "\ndumping the hash table\ntotal elements = %d, bytes = %d\n",
               hash_table.size, hash_table.size * sizeof *hash_table.table);
    
      for (i = hash_table.size - 1; i >= 0; i--)
        if (hash_table.table[i])
          fprintf (stderr, "location[%d] has charset \"%s\" and keyword \"%s\"\n",
                   i, hash_table.table[i]->char_set, hash_table.table[i]->key);

#ifdef GATHER_STATISTICS
      fprintf (stderr, "\ntotal collisions during hashing = %d\n", collisions);
#endif
      fprintf (stderr, "end dumping hash table\n\n");
    }
}

/* If the ITEM is already in the hash table return the item found
   in the table.  Otherwise inserts the ITEM, and returns FALSE.
   Uses double hashing. */

LIST_NODE *
retrieve (item, ignore_length)
     LIST_NODE *item;
     int        ignore_length;
{
  unsigned hash_val  = hash_pjw (item->char_set);
  int      probe     = hash_val & hash_table.size - 1;
  int      increment = (hash_val ^ item->length | 1) & hash_table.size - 1;
  
  while (hash_table.table[probe]
         && (strcmp (hash_table.table[probe]->char_set, item->char_set)
             || (!ignore_length && hash_table.table[probe]->length != item->length)))
    {
#ifdef GATHER_STATISTICS
      collisions++;
#endif
      probe = probe + increment & hash_table.size - 1;
    }

  if (hash_table.table[probe])
    return hash_table.table[probe];
  else
    {
      hash_table.table[probe] = item;
      return 0;
    }
}


