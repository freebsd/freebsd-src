/* getstr.c -- core function for GNU C library getline replacement function

   Copyright (C) 1993, 1996-2000 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/* Written by Jan Brittenson, bson@gnu.ai.mit.edu.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>

#include <assert.h>

#if STDC_HEADERS
# include <stdlib.h>
#else
char *malloc (), *realloc ();
#endif

/* Always add at least this many bytes when extending the buffer.  */
#define MIN_CHUNK 64

/* Read up to (and including) a delimiter DELIM1 from STREAM into *LINEPTR
   + OFFSET (and NUL-terminate it).  If DELIM2 is non-zero, then read up
   and including the first occurrence of DELIM1 or DELIM2.  *LINEPTR is
   a pointer returned from malloc (or NULL), pointing to *N characters of
   space.  It is realloc'd as necessary.  Return the number of characters
   read (not including the NUL terminator), or -1 on error or EOF.  */

int
getstr (char **lineptr, size_t *n, FILE *stream, int delim1, int delim2,
	size_t offset)
{
  int nchars_avail;		/* Allocated but unused chars in *LINEPTR.  */
  char *read_pos;		/* Where we're reading into *LINEPTR. */
  int ret;

  if (!lineptr || !n || !stream)
    return -1;

  if (!*lineptr)
    {
      *n = MIN_CHUNK;
      *lineptr = malloc (*n);
      if (!*lineptr)
	return -1;
    }

  nchars_avail = *n - offset;
  read_pos = *lineptr + offset;

  for (;;)
    {
      register int c = getc (stream);

      /* We always want at least one char left in the buffer, since we
	 always (unless we get an error while reading the first char)
	 NUL-terminate the line buffer.  */

      assert(*n - nchars_avail == read_pos - *lineptr);
      if (nchars_avail < 2)
	{
	  if (*n > MIN_CHUNK)
	    *n *= 2;
	  else
	    *n += MIN_CHUNK;

	  nchars_avail = *n + *lineptr - read_pos;
	  *lineptr = realloc (*lineptr, *n);
	  if (!*lineptr)
	    return -1;
	  read_pos = *n - nchars_avail + *lineptr;
	  assert(*n - nchars_avail == read_pos - *lineptr);
	}

      if (c == EOF || ferror (stream))
	{
	  /* Return partial line, if any.  */
	  if (read_pos == *lineptr)
	    return -1;
	  else
	    break;
	}

      *read_pos++ = c;
      nchars_avail--;

      if (c == delim1 || (delim2 && c == delim2))
	/* Return the line.  */
	break;
    }

  /* Done - NUL terminate and return the number of chars read.  */
  *read_pos = '\0';

  ret = read_pos - (*lineptr + offset);
  return ret;
}
