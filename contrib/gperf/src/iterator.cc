/* Provides an Iterator for keyword characters.
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

#include "iterator.h"

#include <ctype.h>
#include "trace.h"

/* Constructor for Iterator. */

Iterator::Iterator (const char *s, int lo, int hi, int word_end, int bad_val, int key_end)
{
  T (Trace t ("Iterator::Iterator");)
  end         = key_end;
  error_value = bad_val;
  end_word    = word_end;
  str         = s;
  hi_bound    = hi;
  lo_bound    = lo;
}

/* Provide an Iterator, returning the ``next'' value from
   the list of valid values given in the constructor. */

int
Iterator::operator() (void)
{
  T (Trace t ("Iterator::operator()");)
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
      while (*str)
        switch (*str)
          {
          default: return error_value;
          case ',': str++; break;
          case '$': str++; return end_word;
          case '0': case '1': case '2': case '3': case '4':
          case '5': case '6': case '7': case '8': case '9':
            for (curr_value = 0; isdigit ((unsigned char)(*str)); str++)
              curr_value = curr_value * 10 + (*str - '0');

            if (*str == '-')
              {

                for (size = 1, upper_bound = 0;
                     isdigit ((unsigned char)(*++str));
                     upper_bound = upper_bound * 10 + (*str - '0'));

                if (upper_bound <= curr_value || upper_bound > hi_bound)
                  return error_value;
              }
            return curr_value >= lo_bound && curr_value <= hi_bound
              ? curr_value : error_value;
          }

      return end;
    }
}
