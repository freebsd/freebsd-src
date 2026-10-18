/*
 * stdinc.h
 * pull in standard headers (including portability hacks)
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2012 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef LIBPKGCONF_STDINC_H
#define LIBPKGCONF_STDINC_H

/* make POSIX/BSD declarations (e.g. strdup) visible even under a strict -std= */
#ifndef _DEFAULT_SOURCE
# define _DEFAULT_SOURCE
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>

#if defined(_WIN32) || defined(__OS2__)
# ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
# else
#  include <unistd.h>   /* readlink */
# endif
# include <malloc.h>
# include <io.h>     /* for _setmode() */
# include <fcntl.h>
# define PATH_DEV_NULL	"nul"
# ifdef _MSC_VER
#  if _MSC_VER >= 1900
#   define SIZE_FMT_SPECIFIER "%zu"
#  else
#   ifdef _WIN64
#    define SIZE_FMT_SPECIFIER "%I64u"
#   else
#    define SIZE_FMT_SPECIFIER "%u"
#   endif
#  endif
# else
#  ifdef _WIN64
#   ifndef __MINGW32__
#    define SIZE_FMT_SPECIFIER	"%I64u"
#   else
#    define SIZE_FMT_SPECIFIER	"%llu"
#   endif
#  else
#   define SIZE_FMT_SPECIFIER	"%u"
#  endif
# endif
# if !(defined(ssize_t) || defined(__OS2__))
# ifndef __MINGW32__
#  include <BaseTsd.h>
# else
#  include <basetsd.h>
# endif
#  define ssize_t SSIZE_T
# endif
# if !(defined (__MINGW32__) || defined(__OS2__))
#  include "win-dirent.h"
# else
#  include <dirent.h>
# endif
# define PKGCONF_ITEM_SIZE (_MAX_PATH + 1024)
# define PKG_CONFIG_PATH_SEP_S   ";"
# define PKG_DIR_SEP_S   '\\'
# ifndef __OS2__
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
#  define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
# else
#  define _setmode(h, m) if (!isatty((h))) setmode((h), (m))
# endif
#else
# define PATH_DEV_NULL	"/dev/null"
# define SIZE_FMT_SPECIFIER	"%zu"
# ifdef __HAIKU__
#  include <FindDirectory.h>
# endif
# include <dirent.h>
# include <unistd.h>
# include <strings.h>
# include <fcntl.h>    // open
# include <libgen.h>   // basename/dirname
# include <sys/stat.h> // lstat, S_ISLNK
# include <unistd.h>   // close, readlinkat
# include <string.h>
# ifdef PATH_MAX
#  define PKGCONF_ITEM_SIZE (PATH_MAX + 1024)
# else
#  define PKGCONF_ITEM_SIZE (4096 + 1024)
# endif
# define PKG_CONFIG_PATH_SEP_S   ":"
# define PKG_DIR_SEP_S   '/'
#endif

#endif
