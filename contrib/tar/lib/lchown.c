/* Provide a stub lchown function for systems that lack it.
   Copyright (C) 1998, 1999 Free Software Foundation, Inc.

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

/* written by Jim Meyering */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef errno
extern int errno;
#endif
#include "lchown.h"

#ifdef STAT_MACROS_BROKEN
# undef S_ISLNK
#endif
#if !defined(S_ISLNK) && defined(S_IFLNK)
# define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif

/* Declare chown to avoid a warning.  Don't include unistd.h,
   because it may have a conflicting prototype for lchown.  */
int chown ();

/* Work just like chown, except when FILE is a symbolic link.
   In that case, set errno to ENOSYS and return -1.  */

int
lchown (const char *file, uid_t uid, gid_t gid)
{
  struct stat stats;

  if (lstat (file, &stats) == 0 && S_ISLNK (stats.st_mode))
    {
      errno = ENOSYS;
      return -1;
    }

  return chown (file, uid, gid);
}
