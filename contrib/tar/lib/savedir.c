/* savedir.c -- save the list of files in a directory in a string

   Copyright 1990, 1997, 1998, 1999, 2000, 2001 Free Software
   Foundation, Inc.

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

/* Written by David MacKenzie <djm@gnu.ai.mit.edu>. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#ifdef CLOSEDIR_VOID
/* Fake a return value. */
# define CLOSEDIR(d) (closedir (d), 0)
#else
# define CLOSEDIR(d) closedir (d)
#endif

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif
#ifndef NULL
# define NULL 0
#endif

#include "savedir.h"
#include "xalloc.h"

/* Return a freshly allocated string containing the filenames
   in directory DIR, separated by '\0' characters;
   the end is marked by two '\0' characters in a row.
   Return NULL (setting errno) if DIR cannot be opened, read, or closed.  */

#ifndef NAME_SIZE_DEFAULT
# define NAME_SIZE_DEFAULT 512
#endif

char *
savedir (const char *dir)
{
  DIR *dirp;
  struct dirent *dp;
  char *name_space;
  size_t allocated = NAME_SIZE_DEFAULT;
  size_t used = 0;
  int save_errno;

  dirp = opendir (dir);
  if (dirp == NULL)
    return NULL;

  name_space = xmalloc (allocated);

  errno = 0;
  while ((dp = readdir (dirp)) != NULL)
    {
      /* Skip "", ".", and "..".  "" is returned by at least one buggy
         implementation: Solaris 2.4 readdir on NFS filesystems.  */
      char const *entry = dp->d_name;
      if (entry[entry[0] != '.' ? 0 : entry[1] != '.' ? 1 : 2] != '\0')
	{
	  size_t entry_size = strlen (entry) + 1;
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
  if (CLOSEDIR (dirp) != 0)
    save_errno = errno;
  if (save_errno != 0)
    {
      free (name_space);
      errno = save_errno;
      return NULL;
    }
  return name_space;
}
