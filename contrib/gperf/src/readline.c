/* Correctly reads an arbitrarily size string.

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
#include "readline.h"

/* Size of each chunk. */
#define CHUNK_SIZE BUFSIZ

/* Recursively fills up the buffer. */

static char *
readln_aux (chunks) 
     int chunks;
{
  char *buffered_malloc ();
  char buf[CHUNK_SIZE];
  register char *bufptr = buf;
  register char *ptr;
  int c;

  while ((c = getchar ()) != EOF && c != '\n') /* fill the current buffer */
    {
      *bufptr++ = c;
      if (bufptr - buf >= CHUNK_SIZE) /* prepend remainder to ptr buffer */
        {
          if (ptr = readln_aux (chunks + 1))

            for (; bufptr != buf; *--ptr = *--bufptr);

          return ptr;
        }
    }

  if (c == EOF && bufptr == buf)
    return NULL;

  c = (chunks * CHUNK_SIZE + bufptr - buf) + 1;

  if (ptr = buffered_malloc (c))
    {

      for (*(ptr += (c - 1)) = '\0'; bufptr != buf; *--ptr = *--bufptr)
        ;

      return ptr;
    } 
  else 
    return NULL;
}

/* Returns the ``next'' line, ignoring comments beginning with '#'. */

char *read_line () 
{
  int c;
  if ((c = getchar ()) == '#')
    {
      while ((c = getchar ()) != '\n' && c != EOF)
        ;

      return c != EOF ? read_line () : NULL;
    }
  else
    {
      ungetc (c, stdin);
      return readln_aux (0);
    }
}
