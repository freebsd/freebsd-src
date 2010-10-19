/* bucomm.h -- binutils common include file.
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002, 2003, 2005 Free Software Foundation, Inc.

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef _BUCOMM_H
#define _BUCOMM_H

#include "ansidecl.h"
#include <stdio.h>
#include <sys/types.h>

#include "config.h"
#include "bin-bugs.h"

#include <stdarg.h>

#ifdef USE_BINARY_FOPEN
#include "fopen-bin.h"
#else
#include "fopen-same.h"
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#else
extern char *strchr ();
extern char *strrchr ();
#endif
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#if !HAVE_DECL_STPCPY
extern char *stpcpy (char *, const char *);
#endif

#if !HAVE_DECL_STRSTR
extern char *strstr ();
#endif

#ifdef HAVE_SBRK
#if !HAVE_DECL_SBRK
extern char *sbrk ();
#endif
#endif

#if !HAVE_DECL_GETENV
extern char *getenv ();
#endif

#if !HAVE_DECL_ENVIRON
extern char **environ;
#endif

#if !HAVE_DECL_FPRINTF
extern int fprintf (FILE *, const char *, ...);
#endif

#if !HAVE_DECL_SNPRINTF
extern int snprintf(char *, size_t, const char *, ...);
#endif

#if !HAVE_DECL_VSNPRINTF
extern int vsnprintf(char *, size_t, const char *, va_list);
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

#ifndef O_RDWR
#define O_RDWR 2
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

#if defined(__GNUC__) && !defined(C_ALLOCA)
# undef alloca
# define alloca __builtin_alloca
#else
# if defined(HAVE_ALLOCA_H) && !defined(C_ALLOCA)
#  include <alloca.h>
# else
#  ifndef alloca /* predefined by HP cc +Olibcalls */
#   if !defined (__STDC__) && !defined (__hpux)
char *alloca ();
#   else
void *alloca ();
#   endif /* __STDC__, __hpux */
#  endif /* alloca */
# endif /* HAVE_ALLOCA_H */
#endif


#ifdef HAVE_LOCALE_H
# ifndef ENABLE_NLS
   /* The Solaris version of locale.h always includes libintl.h.  If we have
      been configured with --disable-nls then ENABLE_NLS will not be defined
      and the dummy definitions of bindtextdomain (et al) below will conflict
      with the defintions in libintl.h.  So we define these values to prevent
      the bogus inclusion of libintl.h.  */
#  define _LIBINTL_H
#  define _LIBGETTEXT_H
# endif
# include <locale.h>
#endif

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(String) gettext (String)
# ifdef gettext_noop
#  define N_(String) gettext_noop (String)
# else
#  define N_(String) (String)
# endif
#else
# define gettext(Msgid) (Msgid)
# define dgettext(Domainname, Msgid) (Msgid)
# define dcgettext(Domainname, Msgid, Category) (Msgid)
# define textdomain(Domainname) while (0) /* nothing */
# define bindtextdomain(Domainname, Dirname) while (0) /* nothing */
# define _(String) (String)
# define N_(String) (String)
#endif

/* Used by ar.c and objcopy.c.  */
#define BUFSIZE 8192

/* bucomm.c */

/* Return the filename in a static buffer.  */
const char *bfd_get_archive_filename (bfd *);

void bfd_nonfatal (const char *);

void bfd_fatal (const char *) ATTRIBUTE_NORETURN;

void report (const char *, va_list) ATTRIBUTE_PRINTF(1,0);

void fatal (const char *, ...) ATTRIBUTE_PRINTF_1 ATTRIBUTE_NORETURN;

void non_fatal (const char *, ...) ATTRIBUTE_PRINTF_1;

void set_default_bfd_target (void);

void list_matching_formats (char **);

void list_supported_targets (const char *, FILE *);

void list_supported_architectures (const char *, FILE *);

int display_info (void);

void print_arelt_descr (FILE *, bfd *, bfd_boolean);

char *make_tempname (char *);

bfd_vma parse_vma (const char *, const char *);

off_t get_file_size (const char *);

extern char *program_name;

/* filemode.c */
void mode_string (unsigned long, char *);

/* version.c */
extern void print_version (const char *);

/* rename.c */
extern void set_times (const char *, const struct stat *);

extern int smart_rename (const char *, const char *, int);

/* libiberty.  */
void *xmalloc (size_t);

void *xrealloc (void *, size_t);

#endif /* _BUCOMM_H */
