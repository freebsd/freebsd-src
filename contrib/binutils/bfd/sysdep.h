/* sysdep.h -- handle host dependencies for the BFD library
   Copyright 1995, 96, 97, 98, 99, 2000 Free Software Foundation, Inc.
   Written by Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef BFD_SYSDEP_H
#define BFD_SYSDEP_H

#include "ansidecl.h"

#include "config.h"

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#if !(defined(errno) || defined(_MSC_VER) && defined(_INC_ERRNO))
extern int errno;
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

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef USE_BINARY_FOPEN
#include "fopen-bin.h"
#else
#include "fopen-same.h"
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
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
#ifndef O_ACCMODE
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif

#include "filenames.h"

#ifdef NEED_DECLARATION_STRSTR
extern char *strstr ();
#endif

#ifdef NEED_DECLARATION_MALLOC
extern PTR malloc ();
#endif

#ifdef NEED_DECLARATION_REALLOC
extern PTR realloc ();
#endif

#ifdef NEED_DECLARATION_FREE
extern void free ();
#endif

#ifdef NEED_DECLARATION_GETENV
extern char *getenv ();
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) dgettext (PACKAGE, String)
#ifdef gettext_noop
#define N_(String) gettext_noop (String)
#else
#define N_(String) (String)
#endif
#else
# define gettext(Msgid) (Msgid)
# define dgettext(Domainname, Msgid) (Msgid)
# define dcgettext(Domainname, Msgid, Category) (Msgid)
# define textdomain(Domainname) while (0) /* nothing */
# define bindtextdomain(Domainname, Dirname) while (0) /* nothing */
# define _(String) (String)
# define N_(String) (String)
#endif

#endif /* ! defined (BFD_SYSDEP_H) */
