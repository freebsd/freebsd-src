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

#ifdef __GNUC__
#ifndef alloca
#define alloca __builtin_alloca
#endif
#else
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#else
#ifdef _AIX
/* AIX alloca decl has to be the first thing in the file, bletch! */
 #pragma alloca
#else  /* not _AIX */
#ifdef ALLOCA_IN_STDLIB
 /* then we need do nothing */
#else
char *alloca ();
#endif /* not ALLOCA_IN_STDLIB */
#endif /* not _AIX */
#endif /* not HAVE_ALLOCA_H */
#endif /* not __GNUS__ */

#include <sys/types.h>
#include <sys/stat.h>

#ifdef STAT_MACROS_BROKEN
#undef S_ISBLK
#undef S_ISCHR
#undef S_ISDIR
#undef S_ISREG
#undef S_ISFIFO
#undef S_ISLNK
#undef S_ISSOCK
#undef S_ISMPB
#undef S_ISMPC
#undef S_ISNWK
#endif

/* Not all systems have S_IFMT, but we probably want to use it if we
   do.  See ChangeLog for a more detailed discussion. */

#if !defined(S_ISBLK) && defined(S_IFBLK)
# if defined(S_IFMT)
# define	S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
# else
# define S_ISBLK(m) ((m) & S_IFBLK)
# endif
#endif

#if !defined(S_ISCHR) && defined(S_IFCHR)
# if defined(S_IFMT)
# define	S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
# else
# define S_ISCHR(m) ((m) & S_IFCHR)
# endif
#endif

#if !defined(S_ISDIR) && defined(S_IFDIR)
# if defined(S_IFMT)
# define	S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
# else
# define S_ISDIR(m) ((m) & S_IFDIR)
# endif
#endif

#if !defined(S_ISREG) && defined(S_IFREG)
# if defined(S_IFMT)
# define	S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
# else
# define S_ISREG(m) ((m) & S_IFREG)
# endif
#endif

#if !defined(S_ISFIFO) && defined(S_IFIFO)
# if defined(S_IFMT)
# define	S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
# else
# define S_ISFIFO(m) ((m) & S_IFIFO)
# endif
#endif

#if !defined(S_ISLNK) && defined(S_IFLNK)
# if defined(S_IFMT)
# define	S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
# else
# define S_ISLNK(m) ((m) & S_IFLNK)
# endif
#endif

#if !defined(S_ISSOCK) && defined(S_IFSOCK)
# if defined(S_IFMT)
# define	S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
# else
# define S_ISSOCK(m) ((m) & S_IFSOCK)
# endif
#endif

#if !defined(S_ISMPB) && defined(S_IFMPB) /* V7 */
# if defined(S_IFMT)
# define S_ISMPB(m) (((m) & S_IFMT) == S_IFMPB)
# define S_ISMPC(m) (((m) & S_IFMT) == S_IFMPC)
# else
# define S_ISMPB(m) ((m) & S_IFMPB)
# define S_ISMPC(m) ((m) & S_IFMPC)
# endif
#endif

#if !defined(S_ISNWK) && defined(S_IFNWK) /* HP/UX */
# if defined(S_IFMT)
# define S_ISNWK(m) (((m) & S_IFMT) == S_IFNWK)
# else
# define S_ISNWK(m) ((m) & S_IFNWK)
# endif
#endif

#if !defined(HAVE_MKFIFO)
#define mkfifo(path, mode) (mknod ((path), (mode) | S_IFIFO, 0))
#endif

#ifdef NEED_DECOY_PERMISSIONS        /* OS/2, really */

#define	S_IRUSR S_IREAD
#define	S_IWUSR S_IWRITE
#define	S_IXUSR S_IEXEC
#define	S_IRWXU	(S_IRUSR | S_IWUSR | S_IXUSR)
#define	S_IRGRP S_IREAD
#define	S_IWGRP S_IWRITE
#define	S_IXGRP S_IEXEC
#define	S_IRWXG	(S_IRGRP | S_IWGRP | S_IXGRP)
#define	S_IROTH S_IREAD
#define	S_IWOTH S_IWRITE
#define	S_IXOTH S_IEXEC
#define	S_IRWXO	(S_IROTH | S_IWOTH | S_IXOTH)

#else /* ! NEED_DECOY_PERMISSIONS */

#ifndef S_IRUSR
#define	S_IRUSR 0400
#define	S_IWUSR 0200
#define	S_IXUSR 0100
/* Read, write, and execute by owner.  */
#define	S_IRWXU	(S_IRUSR|S_IWUSR|S_IXUSR)

#define	S_IRGRP	(S_IRUSR >> 3)	/* Read by group.  */
#define	S_IWGRP	(S_IWUSR >> 3)	/* Write by group.  */
#define	S_IXGRP	(S_IXUSR >> 3)	/* Execute by group.  */
/* Read, write, and execute by group.  */
#define	S_IRWXG	(S_IRWXU >> 3)

#define	S_IROTH	(S_IRGRP >> 3)	/* Read by others.  */
#define	S_IWOTH	(S_IWGRP >> 3)	/* Write by others.  */
#define	S_IXOTH	(S_IXGRP >> 3)	/* Execute by others.  */
/* Read, write, and execute by others.  */
#define	S_IRWXO	(S_IRWXG >> 3)
#endif /* !def S_IRUSR */
#endif /* NEED_DECOY_PERMISSIONS */

#if defined(POSIX) || defined(HAVE_UNISTD_H)
#include <unistd.h>
#include <limits.h>
#else
off_t lseek ();
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_IO_H
#include <io.h>
#endif

#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif

#ifdef timezone
#undef timezone /* needed for sgi */
#endif

#ifdef HAVE_SYS_TIMEB_H
#include <sys/timeb.h>
#else
struct timeb {
    time_t		time;		/* Seconds since the epoch	*/
    unsigned short	millitm;	/* Field not used		*/
    short		timezone;
    short		dstflag;	/* Field not used		*/
};
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
**         3.1 If we now have PATHSIZE, define PATH_MAX in terms of that.
**             and ignore the rest.  Since _POSIX_PATH_MAX (checked for
**             next) is the *most* restrictive (smallest) value, if we
**             trust _POSIX_PATH_MAX, several of our buffers are too small.
**         4. If PATH_MAX is still not defined but _POSIX_PATH_MAX is,
**            then define PATH_MAX in terms of _POSIX_PATH_MAX.
**         5. And if even _POSIX_PATH_MAX doesn't exist just put in
**            a reasonable value.
**         *. All in all, this is an excellent argument for using pathconf()
**            when at all possible.  Or better yet, dynamically allocate
**            our buffers and use getcwd() not getwd().
**
**     This works on:
**         Sun Sparc 10        SunOS 4.1.3  &  Solaris 1.2
**         HP 9000/700         HP/UX 8.07   &  HP/UX 9.01
**         Tektronix XD88/10   UTekV 3.2e
**         IBM RS6000          AIX 3.2
**         Dec Alpha           OSF 1 ????
**         Intel 386           BSDI BSD/386
**         Intel 386           SCO OpenServer Release 5
**         Apollo              Domain 10.4
**         NEC                 SVR4
*/

/* On MOST systems this will get you MAXPATHLEN.
   Windows NT doesn't have this file, tho.  */
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifndef PATH_MAX  
#  ifdef MAXPATHLEN
#    define PATH_MAX                 MAXPATHLEN
#  else
#    include <limits.h>
#    ifndef PATH_MAX
#      ifdef PATHSIZE
#         define PATH_MAX               PATHSIZE
#      else /* no PATHSIZE */
#        ifdef _POSIX_PATH_MAX
#          define PATH_MAX             _POSIX_PATH_MAX
#        else
#          define PATH_MAX             1024
#        endif  /* no _POSIX_PATH_MAX */
#      endif  /* no PATHSIZE */
#    endif /* no PATH_MAX   */
#  endif  /* MAXPATHLEN */
#endif  /* PATH_MAX   */


/* The NeXT (without _POSIX_SOURCE, which we don't want) has a utime.h
   which doesn't define anything.  It would be cleaner to have configure
   check for struct utimbuf, but for now I'm checking NeXT here (so I don't
   have to debug the configure check across all the machines).  */
#if defined (HAVE_UTIME_H) && !defined (NeXT)
#include <utime.h>
#elif defined (HAVE_SYS_UTIME_H)
# include <sys/utime.h>
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

#else /* not STDC_HEADERS and not HAVE_STRING_H */
#include <strings.h>
/* memory.h and strings.h conflict on some systems. */
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

#include <errno.h>

/* Not all systems set the same error code on a non-existent-file
   error.  This tries to ask the question somewhat portably.
   On systems that don't have ENOTEXIST, this should behave just like
   x == ENOENT.  "x" is probably errno, of course. */

#ifdef ENOTEXIST
#  ifdef EOS2ERR
#    define existence_error(x) \
     (((x) == ENOTEXIST) || ((x) == ENOENT) || ((x) == EOS2ERR))
#  else
#    define existence_error(x) \
     (((x) == ENOTEXIST) || ((x) == ENOENT))
#  endif
#else
#    define existence_error(x) ((x) == ENOENT)
#endif


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

/* check for POSIX signals */
#if defined(HAVE_SIGACTION) && defined(HAVE_SIGPROCMASK)
# define POSIX_SIGNALS
#endif

/* MINIX 1.6 doesn't properly support sigaction */
#if defined(_MINIX)
# undef POSIX_SIGNALS
#endif

/* If !POSIX, try for BSD.. Reason: 4.4BSD implements these as wrappers */
#if !defined(POSIX_SIGNALS)
# if defined(HAVE_SIGVEC) && defined(HAVE_SIGSETMASK) && defined(HAVE_SIGBLOCK)
#  define BSD_SIGNALS
# endif
#endif

/* Under OS/2, this must be included _after_ stdio.h; that's why we do
   it here. */
#ifdef USE_OWN_TCPIP_H
#include "tcpip.h"
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

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
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

/* Under MS-DOS and its derivatives (like Windows NT), mkdir takes only one
   argument; permission is handled very differently on those systems than in
   in Unix.  So we leave such systems a hook on which they can hang their
   own definitions.  */
#ifndef CVS_MKDIR
#define CVS_MKDIR mkdir
#endif

/* Some file systems are case-insensitive.  If FOLD_FN_CHAR is
   #defined, it maps the character C onto its "canonical" form.  In a
   case-insensitive system, it would map all alphanumeric characters
   to lower case.  Under Windows NT, / and \ are both path component
   separators, so FOLD_FN_CHAR would map them both to /.  */
#ifndef FOLD_FN_CHAR
#define FOLD_FN_CHAR(c) (c)
#define fnfold(filename) (filename)
#define fncmp strcmp
#endif

/* Different file systems have different path component separators.
   For the VMS port we might need to abstract further back than this.  */
#ifndef ISDIRSEP
#define ISDIRSEP(c) ((c) == '/')
#endif


/* On some systems, lines in text files should be terminated with CRLF,
   not just LF, and the read and write routines do this translation
   for you.  LINES_CRLF_TERMINATED is #defined on such systems.
   - OPEN_BINARY is the flag to pass to the open function for
     untranslated I/O.
   - FOPEN_BINARY_READ is the string to pass to fopen to get
     untranslated reading.
   - FOPEN_BINARY_WRITE is the string to pass to fopen to get
     untranslated writing.  */
#if LINES_CRLF_TERMINATED
#define OPEN_BINARY (O_BINARY)
#define FOPEN_BINARY_READ ("rb")
#define FOPEN_BINARY_WRITE ("wb")
#else
#define OPEN_BINARY (0)
#define FOPEN_BINARY_READ ("r")
#define FOPEN_BINARY_WRITE ("w")
#endif
