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

/* $CVSid: @(#)system.h 1.18 94/09/25 $ */

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
#if !defined(HAVE_MKFIFO)
#define mkfifo(path, mode) (mknod ((path), (mode) | S_IFIFO, 0))
#endif

#if defined(POSIX) || defined(HAVE_UNISTD_H)
#include <unistd.h>
#include <limits.h>
#else
off_t lseek ();
#endif

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

#ifndef HAVE_SYS_TIMEB_H
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

#if !defined(HAVE_FTIME) && !defined(HAVE_TIMEZONE)
#if !defined(timezone)
extern long timezone;
#endif
#endif


/*
**  MAXPATHLEN and PATH_MAX
**
**     On most systems MAXPATHLEN is defined in sys/param.h to be 1024. Of
**     those that this is not true, again most define PATH_MAX in limits.h
**     or sys/limits.h which usually gets included by limits.h. On the few
**     remaining systems that neither statement is true, _POSIX_PATH_MAX 
**     is defined.
**
**     So:
**         1. If PATH_MAX is defined just use it.
**         2. If MAXPATHLEN is defined but not PATH_MAX, then define
**            PATH_MAX in terms of MAXPATHLEN.
**         3. If neither is defined, include limits.h and check for
**            PATH_MAX again.
**         4. If PATH_MAX is still not defined but _POSIX_PATH_MAX is,
**            then define PATH_MAX in terms of _POSIX_PATH_MAX.
**         5. And if even _POSIX_PATH_MAX doesn't exist just put in
**            a reasonable value.
**
**     This works on:
**         Sun Sparc 10        SunOS 4.1.3  &  Solaris 1.2
**         HP 9000/700         HP/UX 8.07   &  HP/UX 9.01
**         Tektronix XD88/10   UTekV 3.2e
**         IBM RS6000          AIX 3.2
**         Dec Alpha           OSF 1 ????
**         Intel 386           BSDI BSD/386
**         Apollo              Domain 10.4
**         NEC                 SVR4
*/

/* On MOST systems this will get you MAXPATHLEN */
#include <sys/param.h>

#ifndef PATH_MAX  
#  ifdef MAXPATHLEN
#    define PATH_MAX                 MAXPATHLEN
#  else
#    include <limits.h>
#    ifndef PATH_MAX
#      ifdef _POSIX_PATH_MAX
#        define PATH_MAX             _POSIX_PATH_MAX
#      else
#        define PATH_MAX             1024
#      endif  /* _POSIX_PATH_MAX */
#    endif  /* PATH_MAX   */
#  endif  /* MAXPATHLEN */
#endif  /* PATH_MAX   */




#ifdef HAVE_UTIME_H
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

#if STDC_HEADERS || HAVE_STRING_H
#include <string.h>
/* An ANSI string.h and pre-ANSI memory.h might conflict. */
#if !STDC_HEADERS && HAVE_MEMORY_H
#include <memory.h>
#endif /* not STDC_HEADERS and HAVE_MEMORY_H */

#ifndef index
#define index strchr
#endif /* index */

#ifndef rindex
#define rindex strrchr
#endif /* rindex */

#ifndef bcmp
#define bcmp(s1, s2, n) memcmp ((s1), (s2), (n))
#endif /* bcmp */

#ifndef bzero
#define bzero(s, n) memset ((s), 0, (n))
#endif /* bzero */

#else /* not STDC_HJEADERS and not HAVE_STRING_H */
#include <strings.h>
/* memory.h and strings.h conflict on some systems. */
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

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

#if defined(USG) || defined(POSIX)
char *getcwd ();
#else
char *getwd ();
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#include <sys/file.h>
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

/* unistd.h defines _POSIX_VERSION on POSIX.1 systems.  */
#if defined(DIRENT) || defined(_POSIX_VERSION)
#include <dirent.h>
#define NLENGTH(dirent) (strlen((dirent)->d_name))
#else /* not (DIRENT or _POSIX_VERSION) */
#define dirent direct
#define NLENGTH(dirent) ((dirent)->d_namlen)
#ifdef HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#ifdef HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif 
#ifdef HAVE_NDIR_H
#include <ndir.h>
#endif 
#endif /* not (DIRENT or _POSIX_VERSION) */

/* Convert B 512-byte blocks to kilobytes if K is nonzero,
   otherwise return it unchanged. */
#define convert_blocks(b, k) ((k) ? ((b) + 1) / 2 : (b))

#ifndef S_ISLNK
#define lstat stat
#endif

/*
 * Some UNIX distributions don't include these in their stat.h Defined here
 * because "config.h" is always included last.
 */
#ifndef S_IWRITE
#define	S_IWRITE	0000200		/* write permission, owner */
#endif
#ifndef S_IWGRP
#define	S_IWGRP		0000020		/* write permission, grougroup */
#endif
#ifndef S_IWOTH
#define	S_IWOTH		0000002		/* write permission, other */
#endif

