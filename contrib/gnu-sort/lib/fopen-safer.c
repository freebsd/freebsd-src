/* Invoke fopen, but avoid some glitches.
   Copyright (C) 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Paul Eggert.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <unistd-safer.h>

#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#include <stdio.h>
#include <stdio-safer.h>

/* Like fopen, but do not return stdin, stdout, or stderr.  */

FILE *
fopen_safer (char const *file, char const *mode)
{
  FILE *fp = fopen (file, mode);

  if (fp)
    {
      int fd = fileno (fp);

      if (0 <= fd && fd <= STDERR_FILENO)
	{
	  int f = dup_safer (fd);

	  if (f < 0)
	    {
	      int e = errno;
	      fclose (fp);
	      errno = e;
	      return NULL;
	    }

	  if (fclose (fp) != 0
	      || ! (fp = fdopen (f, mode)))
	    {
	      int e = errno;
	      close (f);
	      errno = e;
	      return NULL;
	    }
	}
    }

  return fp;
}
