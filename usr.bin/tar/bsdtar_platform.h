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

/* FreeBSD-specific definitions. */
#ifdef __FreeBSD__
#include <sys/cdefs.h>  /* For __FBSDID */
#include <paths.h> /* For _PATH_DEFTAPE */

#define	HAVE_CHFLAGS		1
#define	ARCHIVE_STAT_MTIME_NANOS(st)	(st)->st_mtimespec.tv_nsec

#if __FreeBSD_version >= 450002 /* nl_langinfo introduced */
/* nl_langinfo supports D_MD_ORDER (FreeBSD extension) */
#define HAVE_NL_LANGINFO_D_MD_ORDER	1
#endif

#if __FreeBSD__ > 4
#define	HAVE_GETOPT_LONG	1
#define	HAVE_POSIX_ACL		1
#endif

/*
 * We need to be able to display a filesize using printf().  The type
 * and format string here must be compatible with one another and
 * large enough for any file.
 */
#include <inttypes.h> /* for uintmax_t, if it exists */
#ifdef UINTMAX_MAX
#define	BSDTAR_FILESIZE_TYPE	uintmax_t
#define	BSDTAR_FILESIZE_PRINTF	"%ju"
#else
#define	BSDTAR_FILESIZE_TYPE	unsigned long long
#define	BSDTAR_FILESIZE_PRINTF	"%llu"
#endif

#if __FreeBSD__ < 5
typedef	int64_t	id_t;
#endif

#endif /*  __FreeBSD__ */

/* No non-FreeBSD platform will have __FBSDID, so just define it here. */
#ifndef __FreeBSD__
#define	__FBSDID(a)     /* null */
#endif

/* Linux */
#ifdef linux
#define	_FILE_OFFSET_BITS	64	/* For a 64-bit off_t */
#include <stdint.h> /* for uintmax_t */
#define	BSDTAR_FILESIZE_TYPE	uintmax_t
#define	BSDTAR_FILESIZE_PRINTF	"%ju"
/* XXX get fnmatch GNU extensions (FNM_LEADING_DIR)
 * (should probably use AC_FUNC_FNMATCH_GNU once using autoconf...) */
#define	_GNU_SOURCE
#define	_PATH_DEFTAPE	"/dev/st0"
#define	HAVE_GETOPT_LONG	1

#ifdef HAVE_STRUCT_STAT_TIMESPEC
/* Fetch the nanosecond portion of the timestamp from a struct stat pointer. */
#define	ARCHIVE_STAT_MTIME_NANOS(pstat)	(pstat)->st_mtim.tv_nsec
#else
/* High-res timestamps aren't available, so just use stubs here. */
#define	ARCHIVE_STAT_MTIME_NANOS(pstat)	0
#endif

#endif

/*
 * XXX TODO: Use autoconf to handle non-FreeBSD platforms.
 *
 * #if !defined(__FreeBSD__)
 *    #include "config.h"
 * #endif
 */

#endif /* !BSDTAR_PLATFORM_H_INCLUDED */
