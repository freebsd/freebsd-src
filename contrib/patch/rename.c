/* BSD compatible rename and directory rename function for System V.
   Copyright (C) 1988, 1990 Free Software Foundation, Inc.

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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef errno
extern int errno;
#endif

#if STAT_MACROS_BROKEN
# undef S_ISDIR
#endif

#if !defined(S_ISDIR) && defined(S_IFDIR)
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

/* Rename file FROM to file TO.
   Return 0 if successful, -1 if not. */

int
rename (from, to)
     char *from;
     char *to;
{
  struct stat from_stats, to_stats;
  int pid, status;

  if (stat (from, &from_stats))
    return -1;

  /* Be careful not to unlink `from' if it happens to be equal to `to' or
     (on filesystems that silently truncate filenames after 14 characters)
     if `from' and `to' share the significant characters. */
  if (stat (to, &to_stats))
    {
      if (errno != ENOENT)
        return -1;
    }
  else
    {
      if ((from_stats.st_dev == to_stats.st_dev)
          && (from_stats.st_ino == to_stats.st_ino))
        /* `from' and `to' designate the same file on that filesystem. */
        return 0;

      if (unlink (to) && errno != ENOENT)
        return -1;
    }

#ifdef MVDIR

/* If MVDIR is defined, it should be the full filename of a setuid root
   program able to link and unlink directories.  If MVDIR is not defined,
   then the capability of renaming directories may be missing.  */

  if (S_ISDIR (from_stats.st_mode))
    {
      /* Need a setuid root process to link and unlink directories. */
      pid = fork ();
      switch (pid)
	{
	case -1:		/* Error. */
	  error (1, errno, "cannot fork");

	case 0:			/* Child. */
	  execl (MVDIR, "mvdir", from, to, (char *) 0);
	  error (255, errno, "cannot run `%s'", MVDIR);

	default:		/* Parent. */
	  while (wait (&status) != pid)
	    /* Do nothing. */ ;

	  errno = 0;		/* mvdir printed the system error message. */
	  if (status)
	    return -1;
	}
    }
  else

#endif /* MVDIR */

    {
      if (link (from, to))
	return -1;
      if (unlink (from) && errno != ENOENT)
	{
	  unlink (to);
	  return -1;
	}
    }
  return 0;
}
