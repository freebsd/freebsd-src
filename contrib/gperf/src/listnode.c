/* Creates and initializes a new list node.
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
#include "options.h"
#include "listnode.h"
#include "stderr.h"

/* See comments in perfect.cc. */
extern int occurrences[ALPHABET_SIZE]; 

/* Sorts the key set alphabetically to speed up subsequent operations.
   Uses insertion sort since the set is probably quite small. */

static void 
set_sort (base, len)
     char *base;
     int len;
{
  int i, j;

  for (i = 0, j = len - 1; i < j; i++)
    {
      char curr, tmp;
      
      for (curr = i + 1, tmp = base[curr]; curr > 0 && tmp < base[curr-1]; curr--)
        base[curr] = base[curr - 1];

      base[curr] = tmp;

    }
}

/* Initializes a List_Node.  This requires obtaining memory for the KEY_SET
   initializing them using the information stored in the
   KEY_POSITIONS array in Options, and checking for simple errors.
   It's important to note that KEY and REST are both pointers to
   the different offsets into the same block of dynamic memory pointed to
   by parameter K. The data member REST is used to store any additional fields 
   of the input file (it is set to the "" string if Option[TYPE] is not enabled).
   This is useful if the user wishes to incorporate a lookup structure,
   rather than just an array of keys. */

LIST_NODE *
make_list_node (k, len)
     char *k;
     int len;
{
	LIST_NODE *buffered_malloc ();
  int char_set_size = OPTION_ENABLED (option, ALLCHARS) ? len : GET_CHARSET_SIZE (option) + 1;
  LIST_NODE *temp = buffered_malloc (sizeof (LIST_NODE) + char_set_size);
  char *ptr = temp->char_set;

  k[len]       = '\0';        /* Null terminate KEY to separate it from REST. */
  temp->key    = k;
  temp->next   = 0;
  temp->index  = 0;
  temp->length = len;
  temp->link   = 0;
  temp->rest   = OPTION_ENABLED (option, TYPE) ? k + len + 1 : "";

  if (OPTION_ENABLED (option, ALLCHARS)) /* Use all the character position in the KEY. */

    for (; *k; k++, ptr++)
      ++occurrences[*ptr = *k];

  else                          /* Only use those character positions specified by the user. */
    {                           
      int i;

      /* Iterate thru the list of key_positions, initializing occurrences table
         and temp->char_set (via char * pointer ptr). */

      for(RESET (option); (i = GET (option)) != EOS; )
        {
          if (i == WORD_END)    /* Special notation for last KEY position, i.e. '$'. */
            *ptr = temp->key[len - 1];
          else if (i <= len)    /* Within range of KEY length, so we'll keep it. */
            *ptr = temp->key[i - 1];
          else                  /* Out of range of KEY length, so we'll just skip it. */
            continue;
          ++occurrences[*ptr++];
        }

      if (ptr == temp->char_set) /* Didn't get any hits, i.e., no usable positions. */
        report_error ("can't hash keyword %s with chosen key positions\n%a", temp->key);
    }

  *ptr = '\0';                  /* Terminate this bastard.... */
  /* Sort the KEY_SET items alphabetically. */
  set_sort (temp->char_set, ptr - temp->char_set); 

  return temp;
}
