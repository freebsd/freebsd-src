/* Portability declarations.  Requires sys/types.h.
   Copyright (C) 1988, 1992 Free Software Foundation

This file is part of GNU Tar.

GNU Tar is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Tar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Tar; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* $FreeBSD$ */

/* AIX requires this to be the first thing in the file. */
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not __GNUC__ */
#if HAVE_ALLOCA_H
#include <alloca.h>
#else /* not HAVE_ALLOCA_H */
#ifdef _AIX
 #pragma alloca
#else /* not _AIX */
char *alloca ();
#endif /* not _AIX */
#endif /* not HAVE_ALLOCA_H */
#endif /* not __GNUC__ */

#include "pathmax.h"

#ifdef _POSIX_VERSION
#include <sys/wait.h>
#else /* !_POSIX_VERSION */
#define WIFSTOPPED(w) (((w) & 0xff) == 0x7f)
#define WIFSIGNALED(w) (((w) & 0xff) != 0x7f && ((w) & 0xff) != 0)
#define WIFEXITED(w) (((w) & 0xff) == 0)

#define WSTOPSIG(w) (((w) >> 8) & 0xff)
#define WTERMSIG(w) ((w) & 0x7f)
#define WEXITSTATUS(w) (((w) >> 8) & 0xff)
#endif /* _POSIX_VERSION */

/* nonstandard */
#ifndef WIFCOREDUMPED
#define WIFCOREDUMPED(w) (((w) & 0x80) != 0)
#endif

#ifdef __MSDOS__
/* missing things from sys/stat.h */
#define	S_ISUID		0
#define	S_ISGID		0
#define	S_ISVTX		0

/* device stuff */
#define	makedev(ma, mi)		((ma << 8) | mi)
#define	major(dev)		(dev)
#define	minor(dev)		(dev)
typedef long off_t;
#endif /* __MSDOS__ */

#if defined(__STDC__) || defined(__TURBOC__)
#define PTR void *
#else
#define PTR char *
#define const
#endif

/* Since major is a function on SVR4, we can't just use `ifndef major'.  */
#ifdef major			/* Might be defined in sys/types.h.  */
#define HAVE_MAJOR
#endif

#if !defined(HAVE_MAJOR) && defined(MAJOR_IN_MKDEV)
#include <sys/mkdev.h>
#define HAVE_MAJOR
#endif

#if !defined(HAVE_MAJOR) && defined(MAJOR_IN_SYSMACROS)
#include <sys/sysmacros.h>
#define HAVE_MAJOR
#endif

#ifndef HAVE_MAJOR
#define major(dev)  (((dev) >> 8) & 0xff)
#define minor(dev)  ((dev) & 0xff)
#define makedev(maj, min)  (((maj) << 8) | (min))
#endif
#undef HAVE_MAJOR

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#include <string.h>
#if !defined(__MSDOS__) && !defined(STDC_HEADERS)
#include <memory.h>
#endif
#ifdef index
#undef index
#endif
#ifdef rindex
#undef rindex
#endif
#define index strchr
#define rindex strrchr
#define bcopy(s, d, n) memcpy(d, s, n)
#define bzero(s, n) memset(s, 0, n)
#define bcmp memcmp
#else
#include <strings.h>
#endif

#if defined(STDC_HEADERS)
#include <stdlib.h>
#else
char *malloc (), *realloc ();
char *getenv ();
#endif
PTR ck_malloc ();
PTR ck_realloc ();
char *xmalloc ();

#ifndef _POSIX_VERSION
#ifdef __MSDOS__
#include <io.h>
#else /* !__MSDOS__ */
off_t lseek ();
#endif /* !__MSDOS__ */
char *getcwd ();
#endif /* !_POSIX_VERSION */

#ifndef NULL
#define NULL 0
#endif

#ifndef	O_BINARY
#define	O_BINARY	0
#endif
#ifndef O_CREAT
#define O_CREAT		0
#endif
#ifndef	O_NDELAY
#define	O_NDELAY	0
#endif
#ifndef	O_RDONLY
#define	O_RDONLY	0
#endif
#ifndef O_RDWR
#define O_RDWR		2
#endif

#include <sys/stat.h>
#ifndef S_ISREG			/* Doesn't have POSIX.1 stat stuff. */
#define mode_t unsigned short
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
#define mkfifo(path, mode) (mknod ((path), (mode) | S_IFIFO, 0))
#endif
#if !defined(S_ISLNK) && defined(S_IFLNK)
#define	S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif
#if !defined(S_ISSOCK) && defined(S_IFSOCK)
#define	S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#endif
#if !defined(S_ISMPB) && defined(S_IFMPB)	/* V7 */
#define S_ISMPB(m) (((m) & S_IFMT) == S_IFMPB)
#define S_ISMPC(m) (((m) & S_IFMT) == S_IFMPC)
#endif
#if !defined(S_ISNWK) && defined(S_IFNWK)	/* HP/UX */
#define S_ISNWK(m) (((m) & S_IFMT) == S_IFNWK)
#endif
#if !defined(S_ISCTG) && defined(S_IFCTG)	/* contiguous file */
#define S_ISCTG(m) (((m) & S_IFMT) == S_IFCTG)
#endif
#if !defined(S_ISVTX)
#define S_ISVTX 0001000
#endif

#ifdef __MSDOS__
#include "msd_dir.h"
#define NLENGTH(direct) ((direct)->d_namlen)

#else /* not __MSDOS__ */

#if defined(DIRENT) || defined(_POSIX_VERSION)
#include <dirent.h>
#define NLENGTH(direct) (strlen((direct)->d_name))
#else /* not (DIRENT or _POSIX_VERSION) */
#define dirent direct
#define NLENGTH(direct) ((direct)->d_namlen)
#ifdef SYSNDIR
#include <sys/ndir.h>
#endif /* SYSNDIR */
#ifdef SYSDIR
#include <sys/dir.h>
#endif /* SYSDIR */
#ifdef NDIR
#include <ndir.h>
#endif /* NDIR */
#endif /* DIRENT or _POSIX_VERSION */

#endif /* not __MSDOS__ */
