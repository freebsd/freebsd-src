/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
This file is part of the GNU C Library.
Contributed by Ian Lance Taylor (ian@airs.com).

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
Cambridge, MA 02139, USA.

Modified by Ian Lanc Taylor for Taylor UUCP, June 1992.  */

#include "uucp.h"

#include "sysdep.h"

#include <errno.h>

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if HAVE_OPENDIR
#if HAVE_DIRENT_H
#include <dirent.h>
#else /* ! HAVE_DIRENT_H */
#include <sys/dir.h>
#define dirent direct
#endif /* ! HAVE_DIRENT_H */
#endif /* HAVE_OPENDIR */

#if HAVE_FTW_H
#include <ftw.h>
#endif

#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 1024
#endif
#endif

/* Traverse one level of a directory tree.  */

static int
ftw_dir (dirs, level, descriptors, dir, len, func)
     DIR **dirs;
     int level;
     int descriptors;
     char *dir;
     size_t len;
     int (*func) P((const char *file, const struct stat *status, int flag));
{
  int got;
  struct dirent *entry;

  got = 0;

  errno = 0;

  while ((entry = readdir (dirs[level])) != NULL)
    {
      size_t namlen;
      struct stat s;
      int flag, ret, newlev;

      ++got;

      namlen = strlen (entry->d_name);
      if (entry->d_name[0] == '.'
	  && (namlen == 1 ||
	      (namlen == 2 && entry->d_name[1] == '.')))
	{
	  errno = 0;
	  continue;
	}

      if (namlen + len + 1 > PATH_MAX)
	{
#ifdef ENAMETOOLONG
	  errno = ENAMETOOLONG;
#else
	  errno = ENOMEM;
#endif
	  return -1;
	}

      dir[len] = '/';
      memcpy ((dir + len + 1), entry->d_name, namlen + 1);

      if (stat (dir, &s) < 0)
	{
	  if (errno != EACCES)
	    return -1;
	  flag = FTW_NS;
	}
      else if (S_ISDIR (s.st_mode))
	{
	  newlev = (level + 1) % descriptors;

	  if (dirs[newlev] != NULL)
	    closedir (dirs[newlev]);

	  dirs[newlev] = opendir (dir);
	  if (dirs[newlev] != NULL)
	    flag = FTW_D;
	  else
	    {
	      if (errno != EACCES)
		return -1;
	      flag = FTW_DNR;
	    }
	}
      else
	flag = FTW_F;

      ret = (*func) (dir, &s, flag);

      if (flag == FTW_D)
	{
	  if (ret == 0)
	    ret = ftw_dir (dirs, newlev, descriptors, dir,
			   namlen + len + 1, func);
	  if (dirs[newlev] != NULL)
	    {
	      int save;

	      save = errno;
	      closedir (dirs[newlev]);
	      errno = save;
	      dirs[newlev] = NULL;
	    }
	}

      if (ret != 0)
	return ret;

      if (dirs[level] == NULL)
	{
	  int skip;

	  dir[len] = '\0';
	  dirs[level] = opendir (dir);
	  if (dirs[level] == NULL)
	    return -1;
	  skip = got;
	  while (skip-- != 0)
	    {
	      errno = 0;
	      if (readdir (dirs[level]) == NULL)
		return errno == 0 ? 0 : -1;
	    }
	}

      errno = 0;
    }

  return errno == 0 ? 0 : -1;
}

/* Call a function on every element in a directory tree.  */

int
ftw (dir, func, descriptors)
     const char *dir;
     int (*func) P((const char *file, const struct stat *status, int flag));
     int descriptors;
{
  DIR **dirs;
  int c;
  DIR **p;
  size_t len;
  char buf[PATH_MAX + 1];
  struct stat s;
  int flag, ret;

  if (descriptors <= 0)
    descriptors = 1;

  dirs = (DIR **) malloc (descriptors * sizeof (DIR *));
  if (dirs == NULL)
    return -1;
  c = descriptors;
  p = dirs;
  while (c-- != 0)
    *p++ = NULL;

  len = strlen (dir);
  memcpy (buf, dir, len + 1);

  if (stat (dir, &s) < 0)
    {
      if (errno != EACCES)
	{
	  free ((pointer) dirs);
	  return -1;
	}
      flag = FTW_NS;
    }
  else if (S_ISDIR (s.st_mode))
    {
      dirs[0] = opendir (dir);
      if (dirs[0] != NULL)
	flag = FTW_D;
      else
	{
	  if (errno != EACCES)
	    {
	      free ((pointer) dirs);
	      return -1;
	    }
	  flag = FTW_DNR;
	}
    }
  else
    flag = FTW_F;

  ret = (*func) (buf, &s, flag);

  if (flag == FTW_D)
    {
      if (ret == 0)
	ret = ftw_dir (dirs, 0, descriptors, buf, len, func);
      if (dirs[0] != NULL)
	{
	  int save;

	  save = errno;
	  closedir (dirs[0]);
	  errno = save;
	}
    }

  free ((pointer) dirs);
  return ret;
}
