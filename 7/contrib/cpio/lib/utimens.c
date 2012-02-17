/* Set file access and modification times.

   Copyright (C) 2003, 2004, 2005, 2006, 2007 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by Paul Eggert.  */

/* derived from a function in touch.c */

#include <config.h>

#include "utimens.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#if HAVE_UTIME_H
# include <utime.h>
#endif

/* Some systems (even some that do have <utime.h>) don't declare this
   structure anywhere.  */
#ifndef HAVE_STRUCT_UTIMBUF
struct utimbuf
{
  long actime;
  long modtime;
};
#endif

/* Some systems don't have ENOSYS.  */
#ifndef ENOSYS
# ifdef ENOTSUP
#  define ENOSYS ENOTSUP
# else
/* Some systems don't have ENOTSUP either.  */
#  define ENOSYS EINVAL
# endif
#endif

#ifndef __attribute__
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 8) || __STRICT_ANSI__
#  define __attribute__(x)
# endif
#endif

#ifndef ATTRIBUTE_UNUSED
# define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#endif

/* Set the access and modification time stamps of FD (a.k.a. FILE) to be
   TIMESPEC[0] and TIMESPEC[1], respectively.
   FD must be either negative -- in which case it is ignored --
   or a file descriptor that is open on FILE.
   If FD is nonnegative, then FILE can be NULL, which means
   use just futimes (or equivalent) instead of utimes (or equivalent),
   and fail if on an old system without futimes (or equivalent).
   If TIMESPEC is null, set the time stamps to the current time.
   Return 0 on success, -1 (setting errno) on failure.  */

int
gl_futimens (int fd ATTRIBUTE_UNUSED,
	     char const *file, struct timespec const timespec[2])
{
  /* Some Linux-based NFS clients are buggy, and mishandle time stamps
     of files in NFS file systems in some cases.  We have no
     configure-time test for this, but please see
     <http://bugs.gentoo.org/show_bug.cgi?id=132673> for references to
     some of the problems with Linux 2.6.16.  If this affects you,
     compile with -DHAVE_BUGGY_NFS_TIME_STAMPS; this is reported to
     help in some cases, albeit at a cost in performance.  But you
     really should upgrade your kernel to a fixed version, since the
     problem affects many applications.  */

#if HAVE_BUGGY_NFS_TIME_STAMPS
  if (fd < 0)
    sync ();
  else
    fsync (fd);
#endif

  /* There's currently no interface to set file timestamps with
     nanosecond resolution, so do the best we can, discarding any
     fractional part of the timestamp.  */
#if HAVE_FUTIMESAT || HAVE_WORKING_UTIMES
  struct timeval timeval[2];
  struct timeval const *t;
  if (timespec)
    {
      timeval[0].tv_sec = timespec[0].tv_sec;
      timeval[0].tv_usec = timespec[0].tv_nsec / 1000;
      timeval[1].tv_sec = timespec[1].tv_sec;
      timeval[1].tv_usec = timespec[1].tv_nsec / 1000;
      t = timeval;
    }
  else
    t = NULL;


  if (fd < 0)
    {
# if HAVE_FUTIMESAT
      return futimesat (AT_FDCWD, file, t);
# endif
    }
  else
    {
      /* If futimesat or futimes fails here, don't try to speed things
	 up by returning right away.  glibc can incorrectly fail with
	 errno == ENOENT if /proc isn't mounted.  Also, Mandrake 10.0
	 in high security mode doesn't allow ordinary users to read
	 /proc/self, so glibc incorrectly fails with errno == EACCES.
	 If errno == EIO, EPERM, or EROFS, it's probably safe to fail
	 right away, but these cases are rare enough that they're not
	 worth optimizing, and who knows what other messed-up systems
	 are out there?  So play it safe and fall back on the code
	 below.  */
# if HAVE_FUTIMESAT
      if (futimesat (fd, NULL, t) == 0)
	return 0;
# elif HAVE_FUTIMES
      if (futimes (fd, t) == 0)
	return 0;
# endif
    }
#endif

  if (!file)
    {
#if ! (HAVE_FUTIMESAT || (HAVE_WORKING_UTIMES && HAVE_FUTIMES))
      errno = ENOSYS;
#endif

      /* Prefer EBADF to ENOSYS if both error numbers apply.  */
      if (errno == ENOSYS)
	{
	  int fd2 = dup (fd);
	  int dup_errno = errno;
	  if (0 <= fd2)
	    close (fd2);
	  errno = (fd2 < 0 && dup_errno == EBADF ? EBADF : ENOSYS);
	}

      return -1;
    }

#if HAVE_WORKING_UTIMES
  return utimes (file, t);
#else
  {
    struct utimbuf utimbuf;
    struct utimbuf const *ut;
    if (timespec)
      {
	utimbuf.actime = timespec[0].tv_sec;
	utimbuf.modtime = timespec[1].tv_sec;
	ut = &utimbuf;
      }
    else
      ut = NULL;

    return utime (file, ut);
  }
#endif
}

/* Set the access and modification time stamps of FILE to be
   TIMESPEC[0] and TIMESPEC[1], respectively.  */
int
utimens (char const *file, struct timespec const timespec[2])
{
  return gl_futimens (-1, file, timespec);
}
