/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 * $FreeBSD: src/usr.bin/tar/bsdtar_platform.h,v 1.24 2007/04/12 04:45:32 kientzle Exp $
 */

/*
 * This header is the first thing included in any of the bsdtar
 * source files.  As far as possible, platform-specific issues should
 * be dealt with here and not within individual source files.
 */

#ifndef BSDTAR_PLATFORM_H_INCLUDED
#define	BSDTAR_PLATFORM_H_INCLUDED

#if defined(PLATFORM_CONFIG_H)
/* Use hand-built config.h in environments that need it. */
#include PLATFORM_CONFIG_H
#elif defined(HAVE_CONFIG_H)
/* Most POSIX platforms use the 'configure' script to build config.h */
#include "../config.h"
#else
/* Warn if bsdtar hasn't been (automatically or manually) configured. */
#error Oops: No config.h and no built-in configuration in bsdtar_platform.h.
#endif /* !HAVE_CONFIG_H */

/* No non-FreeBSD platform will have __FBSDID, so just define it here. */
#ifdef __FreeBSD__
#include <sys/cdefs.h>  /* For __FBSDID */
#else
#define	__FBSDID(a)     /* null */
#endif

#ifdef HAVE_LIBARCHIVE
/* If we're using the platform libarchive, include system headers. */
#include <archive.h>
#include <archive_entry.h>
#else
/* Otherwise, include user headers. */
#include "archive.h"
#include "archive_entry.h"
#endif

/*
 * Does this platform have complete-looking POSIX-style ACL support,
 * including some variant of the acl_get_perm() function (which was
 * omitted from the POSIX.1e draft)?
 */
#if HAVE_SYS_ACL_H && HAVE_ACL_PERMSET_T && HAVE_ACL_USER
#if HAVE_ACL_GET_PERM || HAVE_ACL_GET_PERM_NP
#define	HAVE_POSIX_ACL	1
#endif
#endif

#ifdef HAVE_LIBACL
#include <acl/libacl.h>
#endif

#if HAVE_ACL_GET_PERM
#define	ACL_GET_PERM acl_get_perm
#else
#if HAVE_ACL_GET_PERM_NP
#define	ACL_GET_PERM acl_get_perm_np
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
