/* Correctly reads an arbitrarily size string.

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
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111, USA.  */

#include "read-line.h"

#include <stdlib.h>
#include <string.h> /* declares memcpy() */
#include "options.h"
#include "trace.h"

/* Recursively fills up the buffer. */

#define CHUNK_SIZE 4096

/* CHUNKS is the number of chunks (each of size CHUNK_SIZE) which have
   already been read and which are temporarily stored on the stack.
   This function reads the remainder of the line, allocates a buffer
   for the entire line, fills the part beyond &buffer[chunks*CHUNK_SIZE],
   and returns &buffer[chunks*CHUNK_SIZE].  */

char *
Read_Line::readln_aux (int chunks)
{
  T (Trace t ("Read_Line::readln_aux");)
#if LARGE_STACK
  char buf[CHUNK_SIZE];
#else
  // Note: we don't use new, because that invokes a custom operator new.
  char *buf = (char*)malloc(CHUNK_SIZE);
  if (buf == NULL)
    abort ();
#endif
  char *bufptr = buf;
  char *ptr;
  int c;

  while (c = getc (fp), c != EOF && c != '\n') /* fill the current buffer */
    {
      *bufptr++ = c;
      if (bufptr - buf == CHUNK_SIZE)
        {
          if ((ptr = readln_aux (chunks + 1)) != NULL)

            /* prepend remainder to ptr buffer */
            {
              ptr -= CHUNK_SIZE;
              memcpy (ptr, buf, CHUNK_SIZE);
            }

          goto done;
        }
    }
  if (c == EOF && bufptr == buf && chunks == 0)
    ptr = NULL;
  else
    {
      size_t s1 = chunks * CHUNK_SIZE;
      size_t s2 = bufptr - buf;

      ptr = new char[s1+s2+1];
      ptr += s1;
      ptr[s2] = '\0';
      memcpy (ptr, buf, s2);
    }
 done:
#if !LARGE_STACK
  free (buf);
#endif

  return ptr;
}

#ifndef __OPTIMIZE__

#define INLINE /* not inline */
#include "read-line.icc"
#undef INLINE

#endif /* not defined __OPTIMIZE__ */
