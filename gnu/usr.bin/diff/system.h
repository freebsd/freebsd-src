/* System dependent declarations.
   Copyright (C) 1988, 1989, 1992, 1993 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU DIFF; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* We must define `volatile' and `const' first (the latter inside config.h),
   so that they're used consistently in all system includes.  */
#if !__STDC__
#ifndef volatile
#define volatile
#endif
#endif
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#if __STDC__
#define PARAMS(args) args
#define VOID void
#else
#define PARAMS(args) ()
#define VOID char
#endif

#if STAT_MACROS_BROKEN
#undef S_ISBLK
#undef S_ISCHR
#undef S_ISDIR
#undef S_ISFIFO
#undef S_ISREG
#undef S_ISSOCK
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISBLK) && defined(S_IFBLK)
#define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)
#endif
#if !defined(S_ISCHR) && defined(S_IFCHR)
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#endif
#if !defined(S_ISFIFO) && defined(S_IFFIFO)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFFIFO)
#endif
#if !defined(S_ISSOCK) && defined(S_IFSOCK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)
#endif

#ifndef S_IXOTH
#define S_IXOTH 1
#endif
#ifndef S_IXGRP
#define S_IXGRP (S_IXOTH << 3)
#endif
#ifndef S_IXUSR
#define S_IXUSR (S_IXGRP << 3)
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#if HAVE_TIME_H
#include <time.h>
#else
#include <sys/time.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#include <sys/file.h>
#endif

#if !HAVE_DUP2
#define dup2(f,t)	(close (t),  fcntl (f,F_DUPFD,t))
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

#if HAVE_SYS_WAIT_H
#ifndef _POSIX_VERSION
/* Prevent the NeXT prototype using union wait from causing problems.  */
#define wait system_wait
#endif
#include <sys/wait.h>
#ifndef _POSIX_VERSION
#undef wait
#endif
#endif /* HAVE_SYS_WAIT_H */

#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#undef WIFEXITED		/* Avoid 4.3BSD incompatibility with Posix.  */
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#if HAVE_ST_BLKSIZE
#define STAT_BLOCKSIZE(s) (s).st_blksize
#else
#define STAT_BLOCKSIZE(s) (8 * 1024)
#endif

#if DIRENT || defined (_POSIX_VERSION)
#include <dirent.h>
#else /* ! (DIRENT || defined (_POSIX_VERSION)) */
#if SYSNDIR
#include <sys/ndir.h>
#else
#if SYSDIR
#include <sys/dir.h>
#else
#include <ndir.h>
#endif
#endif
#ifdef dirent
#undef dirent
#endif
#define dirent direct
#endif /* ! (DIRENT || defined (_POSIX_VERSION)) */

#if HAVE_VFORK_H
#include <vfork.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#else
VOID *malloc ();
VOID *realloc ();
#endif
#ifndef getenv
char *getenv ();
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#if HAVE_STRING_H
#include <string.h>
#ifndef bzero
#define bzero(s,n) memset (s,0,n)
#endif
#else /* !HAVE_STRING_H */
#include <strings.h>
#ifndef strchr
#define strchr index
#endif
#ifndef strrchr
#define strrchr rindex
#endif
#ifndef memcpy
#define memcpy(d,s,n) bcopy (s,d,n)
#endif
#ifndef memcmp
#define memcmp(s1,s2,n) bcmp (s1,s2,n)
#endif
#endif /* !HAVE_STRING_H */
#if !HAVE_MEMCHR
char *memchr ();
#endif

#include <errno.h>
#if !STDC_HEADERS
extern int errno;
#endif

#define min(a,b) ((a) <= (b) ? (a) : (b))
#define max(a,b) ((a) >= (b) ? (a) : (b))
