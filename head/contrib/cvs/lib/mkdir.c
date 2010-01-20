/* mkrmdir.c -- BSD compatible directory functions for System V
   Copyright (C) 1988, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef STDC_HEADERS
extern int errno;
#endif

/* mkdir and rmdir adapted from GNU tar. */

/* Make directory DPATH, with permission mode DMODE.

   Written by Robert Rother, Mariah Corporation, August 1985
   (sdcsvax!rmr or rmr@uscd).  If you want it, it's yours.

   Severely hacked over by John Gilmore to make a 4.2BSD compatible
   subroutine.	11Mar86; hoptoad!gnu

   Modified by rmtodd@uokmax 6-28-87 -- when making an already existing dir,
   subroutine didn't return EEXIST.  It does now. */

int
mkdir (dpath, dmode)
     const char *dpath;
     int dmode;
{
  int cpid, status;
  struct stat statbuf;

  if (stat (dpath, &statbuf) == 0)
    {
      errno = EEXIST;		/* stat worked, so it already exists. */
      return -1;
    }

  /* If stat fails for a reason other than non-existence, return error. */
  if (! existence_error (errno))
    return -1;

  cpid = fork ();
  switch (cpid)
    {
    case -1:			/* Cannot fork. */
      return -1;		/* errno is set already. */

    case 0:			/* Child process. */
      /* Cheap hack to set mode of new directory.  Since this child
	 process is going away anyway, we zap its umask.
	 This won't suffice to set SUID, SGID, etc. on this
	 directory, so the parent process calls chmod afterward. */
      status = umask (0);	/* Get current umask. */
      umask (status | (0777 & ~dmode));	/* Set for mkdir. */
      execl ("/bin/mkdir", "mkdir", dpath, (char *) 0);
      _exit (1);

    default:			/* Parent process. */
      while (wait (&status) != cpid) /* Wait for kid to finish. */
	/* Do nothing. */ ;

      if (status & 0xFFFF)
	{
	  errno = EIO;		/* /bin/mkdir failed. */
	  return -1;
	}
      return chmod (dpath, dmode);
    }
}

/* Remove directory DPATH.
   Return 0 if successful, -1 if not. */

int
rmdir (dpath)
     char *dpath;
{
  int cpid, status;
  struct stat statbuf;

  if (stat (dpath, &statbuf) != 0)
    return -1;			/* stat set errno. */

  if ((statbuf.st_mode & S_IFMT) != S_IFDIR)
    {
      errno = ENOTDIR;
      return -1;
    }

  cpid = fork ();
  switch (cpid)
    {
    case -1:			/* Cannot fork. */
      return -1;		/* errno is set already. */

    case 0:			/* Child process. */
      execl ("/bin/rmdir", "rmdir", dpath, (char *) 0);
      _exit (1);

    default:			/* Parent process. */
      while (wait (&status) != cpid) /* Wait for kid to finish. */
	/* Do nothing. */ ;

      if (status & 0xFFFF)
	{
	  errno = EIO;		/* /bin/rmdir failed. */
	  return -1;
	}
      return 0;
    }
}
