/* Provides an Iterator for keyword characters.
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
#include <ctype.h>
#include "iterator.h"

/* Locally visible ITERATOR object. */

ITERATOR iterator;

/* Constructor for ITERATOR. */

void
iterator_init (s, lo, hi, word_end, bad_val, key_end)
     char *s;
     int lo;
     int hi;
     int word_end;
     int bad_val;
     int key_end;
{
  iterator.end         = key_end;
  iterator.error_value = bad_val;
  iterator.end_word    = word_end;
  iterator.str         = s;
  iterator.hi_bound    = hi;
  iterator.lo_bound    = lo;
}

/* Define several useful macros to clarify subsequent code. */
#define ISPOSDIGIT(X) ((X)<='9'&&(X)>'0')
#define TODIGIT(X) ((X)-'0')

/* Provide an Iterator, returning the ``next'' value from 
   the list of valid values given in the constructor. */

int 
next ()
{ 
/* Variables to record the Iterator's status when handling ranges, e.g., 3-12. */

  static int size;              
  static int curr_value;           
  static int upper_bound;

  if (size) 
    { 
      if (++curr_value >= upper_bound) 
        size = 0;    
      return curr_value; 
    }
  else 
    {
      while (*iterator.str) 
        {
          if (*iterator.str == ',') 
            iterator.str++;
          else if (*iterator.str == '$') 
            {
              iterator.str++;
              return iterator.end_word;
            }
          else if (ISPOSDIGIT (*iterator.str))
            {

              for (curr_value = 0; isdigit (*iterator.str); iterator.str++) 
                curr_value = curr_value * 10 + *iterator.str - '0';

              if (*iterator.str == '-') 
                {

                  for (size = 1, upper_bound = 0; 
                       isdigit (*++iterator.str); 
                       upper_bound = upper_bound * 10 + *iterator.str - '0');

                  if (upper_bound <= curr_value || upper_bound > iterator.hi_bound) 
                    return iterator.error_value;
                }
              return curr_value >= iterator.lo_bound && curr_value <= iterator.hi_bound 
                ? curr_value : iterator.error_value;
            }
          else
            return iterator.error_value;               
        }

      return iterator.end;
    }
}
