/* savedir.c -- save the list of files in a directory in a string

   Copyright (C) 1990, 1997, 1998, 1999, 2000, 2001, 2003, 2004, 2005,
   2006 Free Software Foundation, Inc.

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
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by David MacKenzie <djm@gnu.ai.mit.edu>. */

#include <config.h>

#include "savedir.h"

#include <sys/types.h>

#include <errno.h>

#include <dirent.h>
#ifndef _D_EXACT_NAMLEN
# define _D_EXACT_NAMLEN(dp)	strlen ((dp)->d_name)
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "openat.h"
#include "xalloc.h"

#ifndef NAME_SIZE_DEFAULT
# define NAME_SIZE_DEFAULT 512
#endif

/* The results of opendir() in this file are not used with dirfd and fchdir,
   therefore save some unnecessary work in fchdir.c.  */
#undef opendir
#undef closedir

/* Return a freshly allocated string containing the file names
   in directory DIRP, separated by '\0' characters;
   the end is marked by two '\0' characters in a row.
   Return NULL (setting errno) if DIRP cannot be read or closed.
   If DIRP is NULL, return NULL without affecting errno.  */

static char *
savedirstream (DIR *dirp)
{
  char *name_space;
  size_t allocated = NAME_SIZE_DEFAULT;
  size_t used = 0;
  int save_errno;

  if (dirp == NULL)
    return NULL;

  name_space = xmalloc (allocated);

  for (;;)
    {
      struct dirent const *dp;
      char const *entry;

      errno = 0;
      dp = readdir (dirp);
      if (! dp)
	break;

      /* Skip "", ".", and "..".  "" is returned by at least one buggy
         implementation: Solaris 2.4 readdir on NFS file systems.  */
      entry = dp->d_name;
      if (entry[entry[0] != '.' ? 0 : entry[1] != '.' ? 1 : 2] != '\0')
	{
	  size_t entry_size = _D_EXACT_NAMLEN (dp) + 1;
	  if (used + entry_size < used)
	    xalloc_die ();
	  if (allocated <= used + entry_size)
	    {
	      do
		{
		  if (2 * allocated < allocated)
		    xalloc_die ();
		  allocated *= 2;
		}
	      while (allocated <= used + entry_size);

	      name_space = xrealloc (name_space, allocated);
	    }
	  memcpy (name_space + used, entry, entry_size);
	  used += entry_size;
	}
    }
  name_space[used] = '\0';
  save_errno = errno;
  if (closedir (dirp) != 0)
    save_errno = errno;
  if (save_errno != 0)
    {
      free (name_space);
      errno = save_errno;
      return NULL;
    }
  return name_space;
}

/* Return a freshly allocated string containing the file names
   in directory DIR, separated by '\0' characters;
   the end is marked by two '\0' characters in a row.
   Return NULL (setting errno) if DIR cannot be opened, read, or closed.  */

char *
savedir (char const *dir)
{
  return savedirstream (opendir (dir));
}

/* Return a freshly allocated string containing the file names
   in directory FD, separated by '\0' characters;
   the end is marked by two '\0' characters in a row.
   Return NULL (setting errno) if FD cannot be read or closed.  */

char *
fdsavedir (int fd)
{
  return savedirstream (fdopendir (fd));
}
