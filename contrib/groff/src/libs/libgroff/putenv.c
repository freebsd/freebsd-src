/* Copyright (C) 1991 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/* Hacked slightly by jjc@jclark.com for groff. */

#include <string.h>

#ifdef __STDC__
#include <stddef.h>
typedef void *PTR;
typedef size_t SIZE_T;
#else /* not __STDC__ */
typedef char *PTR;
typedef int SIZE_T;
#endif /* not __STDC__ */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else /* not HAVE_STDLIB_H */
PTR malloc();
#endif /* not HAVE_STDLIB_H */

#ifndef NULL
#define NULL 0
#endif

extern char **environ;

/* Put STRING, which is of the form "NAME=VALUE", in the environment.  */

int putenv(const char *string)
{
  char *name_end = strchr(string, '=');
  SIZE_T size;
  char **ep;

  if (name_end == NULL)
    {
      /* Remove the variable from the environment.  */
      size = strlen(string);
      for (ep = environ; *ep != NULL; ++ep)
	if (!strncmp(*ep, string, size) && (*ep)[size] == '=')
	  {
	    while (ep[1] != NULL)
	      {
		ep[0] = ep[1];
		++ep;
	      }
	    *ep = NULL;
	    return 0;
	  }
    }

  size = 0;
  for (ep = environ; *ep != NULL; ++ep)
    if (!strncmp(*ep, string, name_end - string)
	&& (*ep)[name_end - string] == '=')
      break;
    else
      ++size;

  if (*ep == NULL)
    {
      static char **last_environ = NULL;
      char **new_environ = (char **) malloc((size + 2) * sizeof(char *));
      if (new_environ == NULL)
	return -1;
      (void) memcpy((PTR) new_environ, (PTR) environ, size * sizeof(char *));
      new_environ[size] = (char *) string;
      new_environ[size + 1] = NULL;
      if (last_environ != NULL)
	free((PTR) last_environ);
      last_environ = new_environ;
      environ = new_environ;
    }
  else
    *ep = (char *) string;

  return 0;
}
