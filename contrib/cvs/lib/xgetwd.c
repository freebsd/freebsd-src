/* xgetwd.c -- return current directory with unlimited length
   Copyright (C) 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* Derived from xgetcwd.c in e.g. the GNU sh-utils.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "system.h"

#include <stdio.h>
#include <errno.h>
#ifndef errno
extern int errno;
#endif
#include <sys/types.h>

#ifndef HAVE_GETWD
char *getwd ();
#define GETWD(buf, max) getwd (buf)
#else
char *getcwd ();
#define GETWD(buf, max) getcwd (buf, max)
#endif

/* Amount by which to increase buffer size when allocating more space. */
#define PATH_INCR 32

char *xmalloc ();
char *xrealloc ();

/* Return the current directory, newly allocated, arbitrarily long.
   Return NULL and set errno on error. */

char *
xgetwd ()
{
  char *cwd;
  char *ret;
  unsigned path_max;

  errno = 0;
  path_max = (unsigned) PATH_MAX;
  path_max += 2;		/* The getcwd docs say to do this. */

  cwd = xmalloc (path_max);

  errno = 0;
  while ((ret = GETWD (cwd, path_max)) == NULL && errno == ERANGE)
    {
      path_max += PATH_INCR;
      cwd = xrealloc (cwd, path_max);
      errno = 0;
    }

  if (ret == NULL)
    {
      int save_errno = errno;
      free (cwd);
      errno = save_errno;
      return NULL;
    }
  return cwd;
}
