/* shell.c -- readline utility functions that are normally provided by
	      bash when readline is linked as part of the shell. */

/* Copyright (C) 1997 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 1, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif /* HAVE_STDLIB_H */

extern char *xmalloc (), *xrealloc ();

#if !defined (SHELL)

#ifdef savestring
#undef savestring
#endif

/* Backwards compatibility, now that savestring has been removed from
   all `public' readline header files. */
char *
savestring (s)
     char *s;
{
  return ((char *)strcpy (xmalloc (1 + (int)strlen (s)), (s)));
}

/* Does shell-like quoting using single quotes. */
char *
single_quote (string)
     char *string;
{
  register int c;
  char *result, *r, *s;

  result = (char *)xmalloc (3 + (3 * strlen (string)));
  r = result;
  *r++ = '\'';

  for (s = string; s && (c = *s); s++)
    {
      *r++ = c;

      if (c == '\'')
	{
	  *r++ = '\\';	/* insert escaped single quote */
	  *r++ = '\'';
	  *r++ = '\'';	/* start new quoted string */
	}
    }

  *r++ = '\'';
  *r = '\0';

  return (result);
}

/* Set the environment variables LINES and COLUMNS to lines and cols,
   respectively. */
void
set_lines_and_columns (lines, cols)
     int lines, cols;
{
  char *b;

#if defined (HAVE_PUTENV)
  b = xmalloc (24);
  sprintf (b, "LINES=%d", lines);
  putenv (b);
  b = xmalloc (24);
  sprintf (b, "COLUMNS=%d", cols);
  putenv (b);
#else /* !HAVE_PUTENV */
#  if defined (HAVE_SETENV)
  b = xmalloc (8);
  sprintf (b, "%d", lines);
  setenv ("LINES", b, 1);
  b = xmalloc (8);
  sprintf (b, "%d", cols);
  setenv ("COLUMNS", b, 1);
#  endif /* HAVE_SETENV */
#endif /* !HAVE_PUTENV */
}

char *
get_env_value (varname)
     char *varname;
{
  return ((char *)getenv (varname));
}

#else /* SHELL */
extern char *get_string_value ();

char *
get_env_value (varname)
     char *varname;
{
  return get_string_value (varname);
}	
#endif /* SHELL */
