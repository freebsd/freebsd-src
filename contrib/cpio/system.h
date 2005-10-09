/* System dependent declarations.  Requires sys/types.h.
   Copyright (C) 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#include <string.h>
#ifndef index
#define index	strchr
#endif
#ifndef rindex
#define rindex	strrchr
#endif
#ifndef bcmp
#define bcmp(s1, s2, n)	memcmp ((s1), (s2), (n))
#endif
#ifndef bzero
#define bzero(s, n)	memset ((s), 0, (n))
#endif
#else
#include <strings.h>
#endif

#include <time.h>

#ifdef STDC_HEADERS
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#ifndef _POSIX_VERSION
#if defined(__MSDOS__) && !defined(__GNUC__)
typedef long off_t;
#endif
off_t lseek ();
#endif

/* Since major is a function on SVR4, we can't use `ifndef major'.  */
#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#define HAVE_MAJOR
#endif

#ifdef MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#define HAVE_MAJOR
#endif

#ifdef major			/* Might be defined in sys/types.h.  */
#define HAVE_MAJOR
#endif

#ifndef HAVE_MAJOR
#define major(dev) (((dev) >> 8) & 0xff)
#define minor(dev) ((dev) & 0xff)
#define	makedev(ma, mi) (((ma) << 8) | (mi))
#endif
#undef HAVE_MAJOR

#if defined(__MSDOS__) || defined(_POSIX_VERSION) || defined(HAVE_FCNTL_H)
#include <fcntl.h>
#else
#include <sys/file.h>
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif
#ifdef __EMX__			/* gcc on OS/2.  */
#define EPERM EACCES
#define ENXIO EIO
#endif

#ifdef HAVE_UTIME_H
#include <utime.h>
#else
struct utimbuf
{
  time_t actime;
  time_t modtime;
};
#endif

#ifdef TRUE
#undef TRUE
#endif
#define TRUE 1
#ifdef FALSE
#undef FALSE
#endif
#define FALSE 0

#ifndef __MSDOS__
#define CONSOLE "/dev/tty"
#else
#define CONSOLE "con"
#endif

#if defined(__MSDOS__) && !defined(__GNUC__)
typedef int uid_t;
typedef int gid_t;
#endif

/* On most systems symlink() always creates links with rwxrwxrwx
   protection modes, but on some (HP/UX 8.07; I think maybe DEC's OSF
   on MIPS too) symlink() uses the value of umask, so links' protection modes
   aren't always rwxrwxrwx.  There doesn't seem to be any way to change
   the modes of a link (no system call like, say, lchmod() ), it seems
   the only way to set the modes right is to set umask before calling
   symlink(). */

#ifndef SYMLINK_USES_UMASK
#define UMASKED_SYMLINK(name1,name2,mode)    symlink(name1,name2)
#else
#define UMASKED_SYMLINK(name1,name2,mode)    umasked_symlink(name1,name2,mode)
#endif /* SYMLINK_USES_UMASK */

