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

#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
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
#define STAT_BLOCKSIZE(s) (S_ISREG ((s).st_mode) ? 8192 : 4096)
#endif

#if DIRENT || defined (_POSIX_VERSION)
#include <dirent.h>
#ifdef direct
#undef direct
#endif
#define direct dirent
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
#endif /* ! (DIRENT || defined (_POSIX_VERSION)) */

#if HAVE_VFORK_H
#include <vfork.h>
#endif

#if HAVE_STRING_H || STDC_HEADERS
#include <string.h>
#ifndef index
#define index	strchr
#endif
#ifndef rindex
#define rindex	strrchr
#endif
#ifndef bcopy
#define bcopy(s,d,n)	memcpy (d,s,n)
#endif
#ifndef bcmp
#define bcmp(s1,s2,n)	memcmp (s1,s2,n)
#endif
#ifndef bzero
#define bzero(s,n)	memset (s,0,n)
#endif
#else
#include <strings.h>
#endif
#if !HAVE_MEMCHR && !STDC_HEADERS
char *memchr ();
#endif

#if STDC_HEADERS
#include <stdlib.h>
#include <limits.h>
#else
char *getenv ();
char *malloc ();
char *realloc ();
#if __STDC__ || __GNUC__
#include "limits.h"
#else
#define INT_MAX 2147483647
#define CHAR_BIT 8
#endif
#endif

#include <errno.h>
#if !STDC_HEADERS
extern int errno;
#endif

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#define TRUE		1
#define	FALSE		0

#if !__STDC__
#define volatile
#endif

#define min(a,b) ((a) <= (b) ? (a) : (b))
#define max(a,b) ((a) >= (b) ? (a) : (b))
