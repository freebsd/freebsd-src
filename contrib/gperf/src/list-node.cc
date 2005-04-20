/* Creates and initializes a new list node.
   Copyright (C) 1989-1998, 2000 Free Software Foundation, Inc.
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

#include "list-node.h"

#include <stdio.h>
#include <stdlib.h> /* declares exit() */
#include "options.h"
#include "trace.h"

/* Sorts the key set alphabetically to speed up subsequent operations.
   Uses insertion sort since the set is probably quite small. */

inline void
List_Node::set_sort (char *base, int len)
{
  T (Trace t ("List_Node::set_sort");)
  int i, j;

  for (i = 0, j = len - 1; i < j; i++)
    {
      char curr, tmp;

      for (curr = i + 1, tmp = base[curr]; curr > 0 && tmp < base[curr-1]; curr--)
        base[curr] = base[curr - 1];

      base[curr] = tmp;

    }
}

/* Initializes a List_Node.  This requires obtaining memory for the CHAR_SET
   initializing them using the information stored in the KEY_POSITIONS array in Options,
   and checking for simple errors.  It's important to note that KEY and REST are
   both pointers to the different offsets into the same block of dynamic memory pointed
   to by parameter K. The data member REST is used to store any additional fields
   of the input file (it is set to the "" string if Option[TYPE] is not enabled).
   This is useful if the user wishes to incorporate a lookup structure,
   rather than just an array of keys.  Finally, KEY_NUMBER contains a count
   of the total number of keys seen so far.  This is used to initialize
   the INDEX field to some useful value. */

List_Node::List_Node (const char *k, int len, const char *r):
     link (0), next (0), key (k), key_length (len), rest (r), index (0)
{
  T (Trace t ("List_Node::List_Node");)
  char *key_set = new char[(option[ALLCHARS] ? len : option.get_max_keysig_size ())];
  char *ptr = key_set;
  int i;

  if (option[ALLCHARS])         /* Use all the character positions in the KEY. */
    for (i = len; i > 0; k++, ptr++, i--)
      ++occurrences[(unsigned char)(*ptr = *k)];
  else                          /* Only use those character positions specified by the user. */
    {
      /* Iterate through the list of key_positions, initializing occurrences table
         and char_set (via char * pointer ptr). */

      for (option.reset (); (i = option.get ()) != EOS; )
        {
          if (i == WORD_END)            /* Special notation for last KEY position, i.e. '$'. */
            *ptr = key[len - 1];
          else if (i <= len)    /* Within range of KEY length, so we'll keep it. */
            *ptr = key[i - 1];
          else                  /* Out of range of KEY length, so we'll just skip it. */
            continue;
          ++occurrences[(unsigned char)(*ptr++)];
        }

      /* Didn't get any hits and user doesn't want to consider the
        keylength, so there are essentially no usable hash positions! */
      if (ptr == char_set && option[NOLENGTH])
        {
          fprintf (stderr, "Can't hash keyword %.*s with chosen key positions.\n",
                   key_length, key);
          exit (1);
        }
    }

  /* Sort the KEY_SET items alphabetically. */
  set_sort (key_set, ptr - key_set);

  char_set = key_set;
  char_set_length = ptr - key_set;
}
