/* rename.c -- BSD compatible directory function for System V
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

/* Rename file FROM to file TO.
   Return 0 if successful, -1 if not. */

int
rename (from, to)
     char *from;
     char *to;
{
  struct stat from_stats;
  int pid, status;

  if (stat (from, &from_stats) == 0)
    {
      /* We don't check existence_error because the systems which need it
	 have rename().  */
      if (unlink (to) && errno != ENOENT)
	return -1;
      if ((from_stats.st_mode & S_IFMT) == S_IFDIR)
	{
#ifdef MVDIR
	  /* I don't think MVDIR ever gets defined, but I don't think
	     it matters, because I don't think CVS ever calls rename()
	     on directories.  */

	  /* Need a setuid root process to link and unlink directories. */
	  pid = fork ();
	  switch (pid)
	    {
	    case -1:		/* Error. */
	      error (1, errno, "cannot fork");

	    case 0:		/* Child. */
	      execl (MVDIR, "mvdir", from, to, (char *) 0);
	      error (255, errno, "cannot run `%s'", MVDIR);

	    default:		/* Parent. */
	      while (wait (&status) != pid)
		/* Do nothing. */ ;

	      errno = 0;	/* mvdir printed the system error message. */
	      return status != 0 ? -1 : 0;
	    }
#else /* no MVDIR */
	  error (1, 0, "internal error: cannot move directories");
#endif /* no MVDIR */
	}
      else
	{
	  /* We don't check existence_error because the systems which need it
	     have rename().  */
	  if (link (from, to) == 0 && (unlink (from) == 0 || errno == ENOENT))
	    return 0;
	}
    }
  return -1;
}
