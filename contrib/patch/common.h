/* common definitions for `patch' */

/* $Id: common.h,v 1.18 1997/06/13 06:28:37 eggert Exp $ */

/*
Copyright 1986, 1988 Larry Wall
Copyright 1990, 1991, 1992, 1993, 1997 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef DEBUGGING
#define DEBUGGING 1
#endif

/* We must define `volatile' and `const' first (the latter inside config.h),
   so that they're used consistently in all system includes.  */
#ifndef __STDC__
# ifndef volatile
# define volatile
# endif
#endif

/* Enable support for fseeko and ftello on hosts
   where it is available but is turned off by default.
   This must be defined before any system file is included.  */
#define _LARGEFILE_SOURCE 1

#include <config.h>

#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

#include <sys/stat.h>
#if ! defined S_ISDIR && defined S_IFDIR
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#if ! defined S_ISREG && defined S_IFREG
# define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_IXOTH
#define S_IXOTH 1
#endif
#ifndef S_IWOTH
#define S_IWOTH 2
#endif
#ifndef S_IROTH
#define S_IROTH 4
#endif
#ifndef S_IXGRP
#define S_IXGRP (S_IXOTH << 3)
#endif
#ifndef S_IWGRP
#define S_IWGRP (S_IWOTH << 3)
#endif
#ifndef S_IRGRP
#define S_IRGRP (S_IROTH << 3)
#endif
#ifndef S_IXUSR
#define S_IXUSR (S_IXOTH << 6)
#endif
#ifndef S_IWUSR
#define S_IWUSR (S_IWOTH << 6)
#endif
#ifndef S_IRUSR
#define S_IRUSR (S_IROTH << 6)
#endif

#if HAVE_LIMITS_H
# include <limits.h>
#endif
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif
#ifndef LONG_MIN
#define LONG_MIN (-1-2147483647L)
#endif

#include <ctype.h>
/* CTYPE_DOMAIN (C) is nonzero if the unsigned char C can safely be given
   as an argument to <ctype.h> macros like `isspace'.  */
#if STDC_HEADERS
#define CTYPE_DOMAIN(c) 1
#else
#define CTYPE_DOMAIN(c) ((unsigned) (c) <= 0177)
#endif
#ifndef ISSPACE
#define ISSPACE(c) (CTYPE_DOMAIN (c) && isspace (c))
#endif

#ifndef ISDIGIT
#define ISDIGIT(c) ((unsigned) (c) - '0' <= 9)
#endif


#ifndef FILESYSTEM_PREFIX_LEN
#define FILESYSTEM_PREFIX_LEN(f) 0
#endif

#ifndef ISSLASH
#define ISSLASH(c) ((c) == '/')
#endif


/* constants */

/* AIX predefines these.  */
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#define TRUE 1
#define FALSE 0

/* handy definitions */

#define strEQ(s1,s2) (!strcmp(s1, s2))
#define strnEQ(s1,s2,l) (!strncmp(s1, s2, l))

/* typedefs */

typedef int bool;			/* must promote to itself */
typedef long LINENUM;			/* must be signed */

/* globals */

extern char const program_name[];

XTERN char *buf;			/* general purpose buffer */
XTERN size_t bufsize;			/* allocated size of buf */

XTERN bool using_plan_a;		/* try to keep everything in memory */

XTERN char *inname;
XTERN char *outfile;
XTERN int inerrno;
XTERN int invc;
XTERN struct stat instat;
XTERN bool dry_run;
XTERN bool posixly_correct;

XTERN char const *origprae;
XTERN char const *origbase;

XTERN char const * volatile TMPOUTNAME;
XTERN char const * volatile TMPINNAME;
XTERN char const * volatile TMPPATNAME;

#ifdef DEBUGGING
XTERN int debug;
#else
# define debug 0
#endif
XTERN bool force;
XTERN bool batch;
XTERN bool noreverse;
XTERN int reverse;
XTERN enum { DEFAULT_VERBOSITY, SILENT, VERBOSE } verbosity;
XTERN bool skip_rest_of_patch;
XTERN int strippath;
XTERN bool canonicalize;
XTERN int patch_get;
XTERN int set_time;
XTERN int set_utc;

enum diff
  {
    NO_DIFF,
    CONTEXT_DIFF,
    NORMAL_DIFF,
    ED_DIFF,
    NEW_CONTEXT_DIFF,
    UNI_DIFF
  };

XTERN enum diff diff_type;

XTERN char *revision;			/* prerequisite revision, if any */

#ifdef __STDC__
# define GENERIC_OBJECT void
#else
# define GENERIC_OBJECT char
#endif

#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 6) || __STRICT_ANSI__
# define __attribute__(x)
#endif

#ifndef PARAMS
# ifdef __STDC__
#  define PARAMS(args) args
# else
#  define PARAMS(args) ()
# endif
#endif

GENERIC_OBJECT *xmalloc PARAMS ((size_t));
void fatal_exit PARAMS ((int)) __attribute__ ((noreturn));

#include <errno.h>
#if !STDC_HEADERS && !defined errno
extern int errno;
#endif

#if STDC_HEADERS || HAVE_STRING_H
# include <string.h>
#else
# if !HAVE_MEMCHR
#  define memcmp(s1, s2, n) bcmp (s1, s2, n)
#  define memcpy(d, s, n) bcopy (s, d, n)
GENERIC_OBJECT *memchr ();
# endif
#endif

#if STDC_HEADERS
# include <stdlib.h>
#else
long atol ();
char *getenv ();
GENERIC_OBJECT *malloc ();
GENERIC_OBJECT *realloc ();
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifndef lseek
off_t lseek ();
#endif
#ifndef SEEK_SET
#define SEEK_SET 0
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
#if _LFS_LARGEFILE
  typedef off_t file_offset;
# define file_seek fseeko
# define file_tell ftello
#else
  typedef long file_offset;
# define file_seek fseek
# define file_tell ftell
#endif

#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifndef O_RDONLY
#define O_RDONLY 0
#endif
#ifndef O_WRONLY
#define O_WRONLY 1
#endif
#ifndef O_RDWR
#define O_RDWR 2
#endif
#ifndef _O_BINARY
#define _O_BINARY 0
#endif
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#endif
#ifndef O_CREAT
#define O_CREAT 0
#endif
#ifndef O_TRUNC
#define O_TRUNC 0
#endif

#if HAVE_SETMODE
  XTERN int binary_transput;	/* O_BINARY if binary i/o is desired */
#else
# define binary_transput 0
#endif

#ifndef NULL_DEVICE
#define NULL_DEVICE "/dev/null"
#endif

#ifndef TTY_DEVICE
#define TTY_DEVICE "/dev/tty"
#endif
