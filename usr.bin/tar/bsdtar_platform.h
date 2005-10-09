/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * This header is the first thing included in any of the bsdtar
 * source files.  As far as possible, platform-specific issues should
 * be dealt with here and not within individual source files.
 */

#ifndef BSDTAR_PLATFORM_H_INCLUDED
#define	BSDTAR_PLATFORM_H_INCLUDED

#if HAVE_CONFIG_H
#include "config.h"
#else

#ifdef __FreeBSD__
/* A default configuration for FreeBSD, used if there is no config.h. */
#define	PACKAGE_NAME "bsdtar"

#if __FreeBSD__ > 4
#define	HAVE_ACL_GET_PERM 0
#define	HAVE_ACL_GET_PERM_NP 1
#define	HAVE_ACL_PERMSET_T 1
#endif
#define	HAVE_BZLIB_H 1
#define	HAVE_CHFLAGS 1
#define	HAVE_DIRENT_D_NAMLEN 1
#define	HAVE_DIRENT_H 1
#define	HAVE_D_MD_ORDER 1
#define	HAVE_FCHDIR 1
#define	HAVE_FCNTL_H 1
#define	HAVE_FNMATCH 1
#define	HAVE_FNM_LEADING_DIR 1
#define	HAVE_FTRUNCATE 1
#define	HAVE_GETOPT_LONG 1
#define	HAVE_INTTYPES_H 1
#define	HAVE_LANGINFO_H 1
#define	HAVE_LIBARCHIVE 1
#define	HAVE_LIBBZ2 1
#define	HAVE_LIBZ 1
#define	HAVE_LIMITS_H 1
#define	HAVE_LOCALE_H 1
#define	HAVE_MALLOC 1
#define	HAVE_MEMMOVE 1
#define	HAVE_MEMORY_H 1
#define	HAVE_MEMSET 1
#if __FreeBSD_version >= 450002 /* nl_langinfo introduced */
#define	HAVE_NL_LANGINFO 1
#endif
#define	HAVE_PATHS_H 1
#define	HAVE_SETLOCALE 1
#define	HAVE_STDINT_H 1
#define	HAVE_STDLIB_H 1
#define	HAVE_STRCHR 1
#define	HAVE_STRDUP 1
#define	HAVE_STRERROR 1
#define	HAVE_STRFTIME 1
#define	HAVE_STRINGS_H 1
#define	HAVE_STRING_H 1
#define	HAVE_STRRCHR 1
#define	HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC 1
#define	HAVE_STRUCT_STAT_ST_RDEV 1
#define	HAVE_SYS_ACL_H 1
#define	HAVE_SYS_IOCTL_H 1
#define	HAVE_SYS_PARAM_H 1
#define	HAVE_SYS_STAT_H 1
#define	HAVE_SYS_TYPES_H 1
#define	HAVE_UINTMAX_T 1
#define	HAVE_UNISTD_H 1
#define	HAVE_VPRINTF 1
#define	HAVE_ZLIB_H 1
#define	STDC_HEADERS 1

#else /* !__FreeBSD__ */
/* Warn if bsdtar hasn't been (automatically or manually) configured. */
#error Oops: No config.h and no built-in configuration in bsdtar_platform.h.
#endif /* !__FreeBSD__ */

#endif /* !HAVE_CONFIG_H */

/* No non-FreeBSD platform will have __FBSDID, so just define it here. */
#ifdef __FreeBSD__
#include <sys/cdefs.h>  /* For __FBSDID */
#else
#define	__FBSDID(a)     /* null */
#endif

#ifndef HAVE_LIBARCHIVE
#error Configuration error: did not find libarchive.
#endif

/*
 * Does this platform have complete-looking POSIX-style ACL support,
 * including some variant of the acl_get_perm() function (which was
 * omitted from the POSIX.1e draft)?
 */
#if HAVE_SYS_ACL_H && HAVE_ACL_PERMSET_T
#if HAVE_ACL_GET_PERM || HAVE_ACL_GET_PERM_NP
#define	HAVE_POSIX_ACL	1
#endif
#endif

#if HAVE_ACL_GET_PERM
#define ACL_GET_PERM acl_get_perm
#else
#if HAVE_ACL_GET_PERM_NP
#define ACL_GET_PERM acl_get_perm_np
#endif
#endif

/*
 * Include "dirent.h" (or it's equivalent on several different platforms).
 *
 * This is slightly modified from the GNU autoconf recipe.
 * In particular, FreeBSD includes d_namlen in it's dirent structure,
 * so my configure script includes an explicit test for the d_namlen
 * field.
 */
#if HAVE_DIRENT_H
# include <dirent.h>
# if HAVE_DIRENT_D_NAMLEN
#  define DIRENT_NAMLEN(dirent) (dirent)->d_namlen
# else
#  define DIRENT_NAMLEN(dirent) strlen((dirent)->d_name)
# endif
#else
# define dirent direct
# define DIRENT_NAMLEN(dirent) (dirent)->d_namlen
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


/*
 * We need to be able to display a filesize using printf().  The type
 * and format string here must be compatible with one another and
 * large enough for any file.
 */
#if HAVE_UINTMAX_T
#define	BSDTAR_FILESIZE_TYPE	uintmax_t
#define	BSDTAR_FILESIZE_PRINTF	"%ju"
#else
#if HAVE_UNSIGNED_LONG_LONG
#define	BSDTAR_FILESIZE_TYPE	unsigned long long
#define	BSDTAR_FILESIZE_PRINTF	"%llu"
#else
#define	BSDTAR_FILESIZE_TYPE	unsigned long
#define	BSDTAR_FILESIZE_PRINTF	"%lu"
#endif
#endif

#if HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
#define	ARCHIVE_STAT_CTIME_NANOS(st)	(st)->st_ctimespec.tv_nsec
#define	ARCHIVE_STAT_MTIME_NANOS(st)	(st)->st_mtimespec.tv_nsec
#else
#if HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
#define	ARCHIVE_STAT_CTIME_NANOS(st)	(st)->st_ctim.tv_nsec
#define	ARCHIVE_STAT_MTIME_NANOS(st)	(st)->st_mtim.tv_nsec
#else
#define	ARCHIVE_STAT_CTIME_NANOS(st)	(0)
#define	ARCHIVE_STAT_MTIME_NANOS(st)	(0)
#endif
#endif

#endif /* !BSDTAR_PLATFORM_H_INCLUDED */
