/* save-cwd.c -- Save and restore current working directory.
   Copyright (C) 1995, 1997, 1998 Free Software Foundation, Inc.

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

/* Written by Jim Meyering <meyering@na-net.ornl.gov>.  */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/file.h>
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#ifndef O_DIRECTORY
# define O_DIRECTORY 0
#endif

#include "save-cwd.h"
#include "error.h"

char *xgetcwd PARAMS ((void));

/* Record the location of the current working directory in CWD so that
   the program may change to other directories and later use restore_cwd
   to return to the recorded location.  This function may allocate
   space using malloc (via xgetcwd) or leave a file descriptor open;
   use free_cwd to perform the necessary free or close.  Upon failure,
   no memory is allocated, any locally opened file descriptors are
   closed;  return non-zero -- in that case, free_cwd need not be
   called, but doing so is ok.  Otherwise, return zero.  */

int
save_cwd (struct saved_cwd *cwd)
{
  static int have_working_fchdir = 1;

  cwd->desc = -1;
  cwd->name = NULL;

  if (have_working_fchdir)
    {
#if HAVE_FCHDIR
      cwd->desc = open (".", O_RDONLY | O_DIRECTORY);
      if (cwd->desc < 0)
	{
	  error (0, errno, "cannot open current directory");
	  return 1;
	}

# if __sun__ || sun
      /* On SunOS 4, fchdir returns EINVAL if accounting is enabled,
	 so we have to fall back to chdir.  */
      if (fchdir (cwd->desc))
	{
	  if (errno == EINVAL)
	    {
	      close (cwd->desc);
	      cwd->desc = -1;
	      have_working_fchdir = 0;
	    }
	  else
	    {
	      error (0, errno, "current directory");
	      close (cwd->desc);
	      cwd->desc = -1;
	      return 1;
	    }
	}
# endif /* __sun__ || sun */
#else
# define fchdir(x) (abort (), 0)
      have_working_fchdir = 0;
#endif
    }

  if (!have_working_fchdir)
    {
      cwd->name = xgetcwd ();
      if (cwd->name == NULL)
	{
	  error (0, errno, "cannot get current directory");
	  return 1;
	}
    }
  return 0;
}

/* Change to recorded location, CWD, in directory hierarchy.
   If "saved working directory", NULL))
   */

int
restore_cwd (const struct saved_cwd *cwd, const char *dest, const char *from)
{
  int fail = 0;
  if (cwd->desc >= 0)
    {
      if (fchdir (cwd->desc))
	{
	  error (0, errno, "cannot return to %s%s%s",
		 (dest ? dest : "saved working directory"),
		 (from ? " from " : ""),
		 (from ? from : ""));
	  fail = 1;
	}
    }
  else if (chdir (cwd->name) < 0)
    {
      error (0, errno, "%s", cwd->name);
      fail = 1;
    }
  return fail;
}

void
free_cwd (struct saved_cwd *cwd)
{
  if (cwd->desc >= 0)
    close (cwd->desc);
  if (cwd->name)
    free (cwd->name);
}
