/* system.h: system-dependent declarations; include this first.
   $Id: system.h,v 1.14 1999/07/17 21:11:34 karl Exp $

   Copyright (C) 1997, 98, 99 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef TEXINFO_SYSTEM_H
#define TEXINFO_SYSTEM_H

#define _GNU_SOURCE

#include <config.h>

/* <unistd.h> should be included before any preprocessor test
   of _POSIX_VERSION.  */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifndef HAVE_SETLOCALE
#define setlocale(category,locale) /* empty */
#endif

/* For gettext (NLS).  */
#include <libintl.h>
#define _(String) gettext (String)
#define N_(String) (String)

#ifdef STDC_HEADERS
#define getopt system_getopt
#include <stdlib.h>
#undef getopt
#else
extern char *getenv ();
#endif

/* Don't use bcopy!  Use memmove if source and destination may overlap,
   memcpy otherwise.  */
#ifdef HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#else
# include <strings.h>
char *memchr ();
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif
#ifdef VMS
#include <perror.h>
#endif

#ifndef HAVE_DECL_STRERROR
extern char *strerror ();
#endif

#ifndef HAVE_DECL_STRCASECMP
extern int strcasecmp ();
#endif

#ifndef HAVE_DECL_STRNCASECMP
extern int strncasecmp ();
#endif

#ifndef HAVE_DECL_STRCOLL
extern int strcoll ();
#endif

#include <sys/stat.h>
#if STAT_MACROS_BROKEN
# undef S_ISDIR
#endif
#if !defined(S_ISDIR) && defined(S_IFDIR)
# define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif /* HAVE_SYS_FILE_H */

#ifndef O_RDONLY
/* Since <fcntl.h> is POSIX, prefer that to <sys/fcntl.h>.
   This also avoids some useless warnings on (at least) Linux.  */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else /* not HAVE_FCNTL_H */
#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif /* not HAVE_SYS_FCNTL_H */
#endif /* not HAVE_FCNTL_H */
#endif /* not O_RDONLY */

/* MS-DOS and similar non-Posix systems have some peculiarities:
    - they distinguish between binary and text files;
    - they use both `/' and `\\' as directory separator in file names;
    - they can have a drive letter X: prepended to a file name;
    - they have a separate root directory on each drive;
    - their filesystems are case-insensitive;
    - directories in environment variables (like INFOPATH) are separated
        by `;' rather than `:';
    - text files can have their lines ended either with \n or with \r\n pairs;

   These are all parameterized here except the last, which is
   handled by the source code as appropriate (mostly, in info/).  */
#ifndef O_BINARY
# ifdef _O_BINARY
#  define O_BINARY _O_BINARY
# else
#  define O_BINARY 0
# endif
#endif /* O_BINARY */

#if O_BINARY
# include <io.h>
# ifdef __MSDOS__
#  include <limits.h>
#  ifdef __DJGPP__
#   define HAVE_LONG_FILENAMES(dir)  (pathconf (dir, _PC_NAME_MAX) > 12)
#   define NULL_DEVICE	"/dev/null"
#  else  /* !__DJGPP__ */
#   define HAVE_LONG_FILENAMES(dir)  (0)
#   define NULL_DEVICE	"NUL"
#  endif /* !__DJGPP__ */
#  define SET_SCREEN_SIZE_HELPER terminal_prep_terminal()
#  define DEFAULT_INFO_PRINT_COMMAND ">PRN"
# else   /* !__MSDOS__ */
#  define setmode(f,m)  _setmode(f,m)
#  define HAVE_LONG_FILENAMES(dir)   (1)
#  define NULL_DEVICE	"NUL"
# endif  /* !__MSDOS__ */
# define SET_BINARY(f)  do {if (!isatty(f)) setmode(f,O_BINARY);} while(0)
# define FOPEN_RBIN	"rb"
# define FOPEN_WBIN	"wb"
# define IS_SLASH(c)	((c) == '/' || (c) == '\\')
# define HAVE_DRIVE(n)	((n)[0] && (n)[1] == ':')
# define IS_ABSOLUTE(n)	(IS_SLASH((n)[0]) || ((n)[0] && (n)[1] == ':'))
# define FILENAME_CMP	strcasecmp
# define FILENAME_CMPN	strncasecmp
# define PATH_SEP	";"
# define STRIP_DOT_EXE	1
# define DEFAULT_TMPDIR	"c:/"
# define PIPE_USE_FORK	0
#else  /* not O_BINARY */
# define SET_BINARY(f)	(void)0
# define FOPEN_RBIN	"r"
# define FOPEN_WBIN	"w"
# define IS_SLASH(c)	((c) == '/')
# define HAVE_DRIVE(n)	(0)
# define IS_ABSOLUTE(n)	((n)[0] == '/')
# define FILENAME_CMP	strcmp
# define FILENAME_CMPN	strncmp
# define HAVE_LONG_FILENAMES(dir)   (1)
# define PATH_SEP	":"
# define STRIP_DOT_EXE	0
# ifdef VMS
#  define DEFAULT_TMPDIR "sys$scratch:"
# else
#  define DEFAULT_TMPDIR "/tmp/"
# endif
# define NULL_DEVICE	"/dev/null"
# define PIPE_USE_FORK	1
#endif /* not O_BINARY */

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
/* Some systems don't declare this function in pwd.h. */
struct passwd *getpwnam ();

/* Our library routines not included in any system library.  */
extern void *xmalloc (), *xrealloc ();
extern char *xstrdup ();
extern void xexit ();
extern char *substring ();

/* For convenience.  */
#define STREQ(s1,s2) (strcmp (s1, s2) == 0)

#endif /* TEXINFO_SYSTEM_H */
