/* dstring.c - The dynamic string handling routines used by cpio.
   Copyright (C) 1990, 1991, 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#if defined(HAVE_STRING_H) || defined(STDC_HEADERS)
#include <string.h>
#else
#include <strings.h>
#endif
#include "dstring.h"

#if __STDC__
# define P_(s) s
#else
# define P_(s) ()
#endif
char *xmalloc P_((unsigned n));
char *xrealloc P_((char *p, unsigned n));

/* Initialiaze dynamic string STRING with space for SIZE characters.  */

void
ds_init (string, size)
     dynamic_string *string;
     int size;
{
  string->ds_length = size;
  string->ds_string = (char *) xmalloc (size);
}

/* Expand dynamic string STRING, if necessary, to hold SIZE characters.  */

void
ds_resize (string, size)
     dynamic_string *string;
     int size;
{
  if (size > string->ds_length)
    {
      string->ds_length = size;
      string->ds_string = (char *) xrealloc ((char *) string->ds_string, size);
    }
}

/* Dynamic string S gets a string terminated by the EOS character
   (which is removed) from file F.  S will increase
   in size during the function if the string from F is longer than
   the current size of S.
   Return NULL if end of file is detected.  Otherwise,
   Return a pointer to the null-terminated string in S.  */

char *
ds_fgetstr (f, s, eos)
     FILE *f;
     dynamic_string *s;
     char eos;
{
  int insize;			/* Amount needed for line.  */
  int strsize;			/* Amount allocated for S.  */
  int next_ch;

  /* Initialize.  */
  insize = 0;
  strsize = s->ds_length;

  /* Read the input string.  */
  next_ch = getc (f);
  while (next_ch != eos && next_ch != EOF)
    {
      if (insize >= strsize - 1)
	{
	  ds_resize (s, strsize * 2 + 2);
	  strsize = s->ds_length;
	}
      s->ds_string[insize++] = next_ch;
      next_ch = getc (f);
    }
  s->ds_string[insize++] = '\0';

  if (insize == 1 && next_ch == EOF)
    return NULL;
  else
    return s->ds_string;
}

char *
ds_fgets (f, s)
     FILE *f;
     dynamic_string *s;
{
  return ds_fgetstr (f, s, '\n');
}

char *
ds_fgetname (f, s)
     FILE *f;
     dynamic_string *s;
{
  return ds_fgetstr (f, s, '\0');
}
