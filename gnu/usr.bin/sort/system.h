/* system-dependent definitions for textutils programs.
   Copyright (C) 1989, 1990, 1991 Free Software Foundation, Inc.

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

/* Include sys/types.h before this file.  */

#include <sys/stat.h>

#ifdef	STAT_MACROS_BROKEN
#ifdef S_ISBLK
#undef S_ISBLK
#endif
#ifdef S_ISCHR
#undef S_ISCHR
#endif
#ifdef S_ISDIR
#undef S_ISDIR
#endif
#ifdef S_ISFIFO
#undef S_ISFIFO
#endif
#ifdef S_ISLNK
#undef S_ISLNK
#endif
#ifdef S_ISMPB
#undef S_ISMPB
#endif
#ifdef S_ISMPC
#undef S_ISMPC
#endif
#ifdef S_ISNWK
#undef S_ISNWK
#endif
#ifdef S_ISREG
#undef S_ISREG
#endif
#ifdef S_ISSOCK
#undef S_ISSOCK
#endif
#endif	/* STAT_MACROS_BROKEN.  */

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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef _POSIX_VERSION
off_t lseek ();
#endif

#if defined(HAVE_STRING_H) || defined(STDC_HEADERS)
#if !defined(STDC_HEADERS) && defined(HAVE_MEMORY_H)
#include <memory.h>
#endif
#include <string.h>
#ifndef index
#define index strchr
#endif
#ifndef rindex
#define rindex strrchr
#endif
/* Don't define bcopy; we need one that can handle overlaps.  */
#ifndef bzero
#define bzero(s, n) memset ((s), 0, (n))
#endif
#ifndef bcmp
#define bcmp(s1, s2, n) memcmp ((s1), (s2), (n))
#endif
#else
#include <strings.h>
char *memchr ();
#endif

#include <errno.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#else
char *getenv ();
extern int errno;
#endif

#if defined(HAVE_FCNTL_H) || defined(_POSIX_VERSION)
#include <fcntl.h>
#else
#include <sys/file.h>
#endif

#if !defined(SEEK_SET)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#ifndef _POSIX_SOURCE
#include <sys/param.h>
#endif

/* Get or fake the disk device blocksize.
   Usually defined by sys/param.h (if at all).  */
#if !defined(DEV_BSIZE) && defined(BSIZE)
#define DEV_BSIZE BSIZE
#endif
#if !defined(DEV_BSIZE) && defined(BBSIZE) /* SGI */
#define DEV_BSIZE BBSIZE
#endif
#ifndef DEV_BSIZE
#define DEV_BSIZE 4096
#endif

/* Extract or fake data from a `struct stat'.
   ST_BLKSIZE: Optimal I/O blocksize for the file, in bytes. */
#ifndef HAVE_ST_BLKSIZE
# define ST_BLKSIZE(statbuf) DEV_BSIZE
#else /* HAVE_ST_BLKSIZE */
/* Some systems, like Sequents, return st_blksize of 0 on pipes. */
# define ST_BLKSIZE(statbuf) ((statbuf).st_blksize > 0 \
			       ? (statbuf).st_blksize : DEV_BSIZE)
#endif /* HAVE_ST_BLKSIZE */

#ifndef S_ISLNK
#define lstat stat
#endif

#ifndef RETSIGTYPE
#define RETSIGTYPE void
#endif

#include <ctype.h>

#ifndef isascii
#define isascii(c) 1
#endif

#ifdef isblank
#define ISBLANK(c) (isascii (c) && isblank (c))
#else
#define ISBLANK(c) ((c) == ' ' || (c) == '\t')
#endif
#ifdef isgraph
#define ISGRAPH(c) (isascii (c) && isgraph (c))
#else
#define ISGRAPH(c) (isascii (c) && isprint (c) && !isspace (c))
#endif

#define ISPRINT(c) (isascii (c) && isprint (c))
#define ISDIGIT(c) (isascii (c) && isdigit (c))
#define ISALNUM(c) (isascii (c) && isalnum (c))
#define ISALPHA(c) (isascii (c) && isalpha (c))
#define ISCNTRL(c) (isascii (c) && iscntrl (c))
#define ISLOWER(c) (isascii (c) && islower (c))
#define ISPUNCT(c) (isascii (c) && ispunct (c))
#define ISSPACE(c) (isascii (c) && isspace (c))
#define ISUPPER(c) (isascii (c) && isupper (c))
#define ISXDIGIT(c) (isascii (c) && isxdigit (c))
