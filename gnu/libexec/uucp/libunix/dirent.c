/* dirent.c
   Replacements for opendir, readdir and closedir for the original
   Unix filesystem only.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#include "sysdep.h"

#include <errno.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

/* Simple emulations of opendir/readdir/closedir for systems which
   have the original format of Unix directories.  It's probably better
   to get Doug Gwyn's public domain set of emulation functions.  */

DIR *
opendir (zdir)
     const char *zdir;
{
  int o;
  struct stat s;
  DIR *qret;

  o = open ((char *) zdir, O_RDONLY | O_NOCTTY, 0);
  if (o < 0)
    return NULL;
  if (fcntl (o, F_SETFD, fcntl (o, F_GETFD, 0) | FD_CLOEXEC) < 0
      || fstat (o, &s) < 0)
    {
      int isave;

      isave = errno;
      (void) close (o);
      errno = isave;
      return NULL;
    }
  if (! S_ISDIR (s.st_mode))
    {
      (void) close (o);
      errno = ENOTDIR;
      return NULL;
    }
  qret = (DIR *) xmalloc (sizeof (DIR));
  qret->o = o;
  return qret;
}

struct dirent *
readdir (q)
    DIR *q;
{
  struct direct sdir;
  int cgot;

  do
    {
      cgot = read (q->o, &sdir, sizeof (struct direct));
      if (cgot <= 0)
	return NULL;
      if (cgot != sizeof (struct direct))
	{
	  errno = ENOENT;
	  return NULL;
	}
    }
  while (sdir.d_ino == 0);

  strncpy (q->s.d_name, sdir.d_name, DIRSIZ);
  q->s.d_name[DIRSIZ] = '\0';
  return &q->s;
}

int
closedir (q)
    DIR *q;
{
  int o;

  o = q->o;
  xfree (q);
  return close (o);
}
