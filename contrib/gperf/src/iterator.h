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

/* Provides an Iterator that expands and decodes a control string containing digits
   and ranges, returning an integer every time the generator function is called.
   This is used to decode the user's key position requests.  For example:
   "-k 1,2,5-10,$"  will return 1, 2, 5, 6, 7, 8, 9, 10, and 0 ( representing
   the abstract ``last character of the key'' on successive calls to the
   member function operator ().
   No errors are handled in these routines, they are passed back to the
   calling routines via a user-supplied Error_Value */

#ifndef _iterator_h
#define _iterator_h
#include "prototype.h"

typedef struct iterator 
{
  char *str;                    /* A pointer to the string provided by the user. */
  int   end;                    /* Value returned after last key is processed. */
  int   end_word;               /* A value marking the abstract ``end of word'' ( usually '$'). */
  int   error_value;            /* Error value returned when input is syntactically erroneous. */
  int   hi_bound;               /* Greatest possible value, inclusive. */
  int   lo_bound;               /* Smallest possible value, inclusive. */
} ITERATOR;

extern void iterator_init P ((char *s, int lo, int hi, int word_end, int bad_val, int key_end));
extern int  next P ((void));
#endif /* _iterator_h */
