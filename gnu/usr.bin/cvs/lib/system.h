/* system-dependent definitions for CVS.
   Copyright (C) 1989-1992 Free Software Foundation, Inc.

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

/* @(#)system.h 1.14 92/04/10 */

#include <sys/types.h>
#include <sys/stat.h>
#ifndef S_ISREG			/* Doesn't have POSIX.1 stat stuff. */
#ifndef mode_t
#define mode_t unsigned short
#endif
#endif
#if !defined(S_ISBLK) && defined(S_IFBLK)
#define	S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#endif
#if !defined(S_ISCHR) && defined(S_IFCHR)
#define	S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#endif
#if !defined(S_ISDIR) && defined(S_IFDIR)
#define	S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG) && defined(S_IFREG)
#define	S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISFIFO) && defined(S_IFIFO)
#define	S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif
#if !defined(S_ISLNK) && defined(S_IFLNK)
#define	S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif
#if !defined(S_ISSOCK) && defined(S_IFSOCK)
#define	S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#endif
#if !defined(S_ISMPB) && defined(S_IFMPB) /* V7 */
#define S_ISMPB(m) (((m) & S_IFMT) == S_IFMPB)
#define S_ISMPC(m) (((m) & S_IFMT) == S_IFMPC)
#endif
#if !defined(S_ISNWK) && defined(S_IFNWK) /* HP/UX */
#define S_ISNWK(m) (((m) & S_IFMT) == S_IFNWK)
#endif
#if defined(MKFIFO_MISSING)
#define mkfifo(path, mode) (mknod ((path), (mode) | S_IFIFO, 0))
#endif

#ifdef POSIX
#include <unistd.h>
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX pathconf ("/", _PC_PATH_MAX)
#endif
#else
off_t lseek ();
#endif

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

#ifdef TIMEB_H_MISSING
struct timeb {
    time_t		time;		/* Seconds since the epoch	*/
    unsigned short	millitm;	/* Field not used		*/
#ifdef timezone
    short		tzone;
#else
    short		timezone;
#endif
    short		dstflag;	/* Field not used		*/
};
#else
#include <sys/timeb.h>
#endif

#if defined(FTIME_MISSING) && !defined(HAVE_TIMEZONE)
#if !defined(timezone)
extern char *timezone();
#endif
#endif

#ifndef POSIX
#include <sys/param.h>
#endif

#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX 255
#endif

#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX _POSIX_PATH_MAX
#endif
#endif

#ifdef POSIX
#include <utime.h>
#else
#ifndef ALTOS
struct utimbuf
{
  long actime;
  long modtime;
};
#endif
int utime ();
#endif

#if defined(USG) || defined(STDC_HEADERS)
#include <string.h>
#ifndef STDC_HEADERS
#include <memory.h>
#endif
#ifndef index
#define index strchr
#endif
#ifndef rindex
#define rindex strrchr
#endif
#ifndef bcopy
#define bcopy(from, to, len) memcpy ((to), (from), (len))
#endif
#ifndef bzero
#define bzero(s, n) (void) memset ((s), 0, (n))
#endif
#ifndef bcmp
#define	bcmp(s1, s2, n) memcmp((s1), (s2), (n))
#endif
#else
#include <strings.h>
#endif

#include <errno.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#else
char *getenv ();
char *malloc ();
char *realloc ();
char *calloc ();
extern int errno;
#endif

#ifdef __GNUC__
#ifdef bsdi
#define alloca __builtin_alloca
#endif
#else
#ifdef sparc
#include <alloca.h>
#else
#ifndef _AIX
/* AIX alloca decl has to be the first thing in the file, bletch! */
char *alloca ();
#endif
#endif
#endif

#if defined(USG) || defined(POSIX)
#include <fcntl.h>
char *getcwd ();
#else
#include <sys/file.h>
char *getwd ();
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif
#ifndef F_OK
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#endif

#ifdef DIRENT
#include <dirent.h>
#ifdef direct
#undef direct
#endif
#define direct dirent
#else
#ifdef SYSNDIR
#include <sys/ndir.h>
#else
#ifdef NDIR
#include <ndir.h>
#else /* must be BSD */
#include <sys/dir.h>
#endif
#endif
#endif

/* Convert B 512-byte blocks to kilobytes if K is nonzero,
   otherwise return it unchanged. */
#define convert_blocks(b, k) ((k) ? ((b) + 1) / 2 : (b))

#ifndef S_ISLNK
#define lstat stat
#endif

#ifndef SIGTYPE
#define SIGTYPE void
#endif
