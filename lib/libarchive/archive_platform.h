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
 * This header is the first thing included in any of the libarchive
 * source files.  As far as possible, platform-specific issues should
 * be dealt with here and not within individual source files.  I'm
 * actively trying to minimize #if blocks within the main source,
 * since they obfuscate the code.
 */

#ifndef ARCHIVE_PLATFORM_H_INCLUDED
#define	ARCHIVE_PLATFORM_H_INCLUDED

/* FreeBSD-specific definitions. */
#ifdef __FreeBSD__
#include <sys/cdefs.h>  /* For __FBSDID */
/*
 * Note that SUSv3 says that inttypes.h includes stdint.h.
 * Since inttypes.h predates stdint.h, it's safest to always
 * use inttypes.h instead of stdint.h.
 */
#include <inttypes.h>  /* For int64_t, etc. */

#if __FreeBSD__ > 4
#define	HAVE_POSIX_ACL 1
#endif

#define	HAVE_CHFLAGS 1
#define	HAVE_LUTIMES 1
#define	HAVE_LCHMOD 1
#define	HAVE_LCHOWN 1
#define	HAVE_STRERROR_R 1
#define	ARCHIVE_ERRNO_FILE_FORMAT EFTYPE
#define	ARCHIVE_ERRNO_PROGRAMMER EINVAL
#define	ARCHIVE_ERRNO_MISC (-1)

/* Fetch/set high-resolution time data through a struct stat pointer. */
#define	ARCHIVE_STAT_ATIME_NANOS(st)	(st)->st_atimespec.tv_nsec
#define	ARCHIVE_STAT_CTIME_NANOS(st)	(st)->st_ctimespec.tv_nsec
#define	ARCHIVE_STAT_MTIME_NANOS(st)	(st)->st_mtimespec.tv_nsec
#define	ARCHIVE_STAT_SET_ATIME_NANOS(st, n) (st)->st_atimespec.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_CTIME_NANOS(st, n) (st)->st_ctimespec.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_MTIME_NANOS(st, n) (st)->st_mtimespec.tv_nsec = (n)

/*
 * Older versions of inttypes.h don't have INT64_MAX, etc.  Since
 * SUSv3 requires them to be macros when they are defined, we can
 * easily test for and define them here if necessary.  Someday, we
 * won't have to worry about non-C99-compliant systems.
 */
#ifndef INT64_MAX
/* XXX Is this really necessary? XXX */
#ifdef __i386__
#define	INT64_MAX 0x7fffffffffffffffLL
#define	UINT64_MAX 0xffffffffffffffffULL
#else /* __alpha__ */
#define	INT64_MAX 0x7fffffffffffffffL
#define	UINT64_MAX 0xffffffffffffffffUL
#endif
#endif /* ! INT64_MAX */

#endif /*  __FreeBSD__ */

/* No non-FreeBSD platform will have __FBSDID, so just define it here. */
#ifndef __FreeBSD__
#define	__FBSDID(a)     /* null */
#endif

/* Linux */
#ifdef LINUX
#define	_FILE_OFFSET_BITS	64  /* Needed for 64-bit file size handling. */
#include <inttypes.h>
#define	ARCHIVE_ERRNO_FILE_FORMAT EILSEQ
#define	ARCHIVE_ERRNO_PROGRAMMER EINVAL
#define	ARCHIVE_ERRNO_MISC (-1)
#define	HAVE_EXT2FS_EXT2_FS_H 1
#define	HAVE_LCHOWN 1
#define	HAVE_STRERROR_R 1
#define	STRERROR_R_CHAR_P 1

#ifdef HAVE_STRUCT_STAT_TIMESPEC
/* Fetch the nanosecond portion of the timestamp from a struct stat pointer. */
#define	ARCHIVE_STAT_ATIME_NANOS(pstat)	(pstat)->st_atim.tv_nsec
#define	ARCHIVE_STAT_CTIME_NANOS(pstat)	(pstat)->st_ctim.tv_nsec
#define	ARCHIVE_STAT_MTIME_NANOS(pstat)	(pstat)->st_mtim.tv_nsec
#define	ARCHIVE_STAT_SET_ATIME_NANOS(st, n) (st)->st_atim.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_CTIME_NANOS(st, n) (st)->st_ctim.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_MTIME_NANOS(st, n) (st)->st_mtim.tv_nsec = (n)
#else
/* High-res timestamps aren't available, so just use stubs here. */
#define	ARCHIVE_STAT_ATIME_NANOS(pstat)	0
#define	ARCHIVE_STAT_CTIME_NANOS(pstat)	0
#define	ARCHIVE_STAT_MTIME_NANOS(pstat)	0
#define	ARCHIVE_STAT_SET_ATIME_NANOS(st, n)
#define	ARCHIVE_STAT_SET_CTIME_NANOS(st, n)
#define	ARCHIVE_STAT_SET_MTIME_NANOS(st, n)
#endif

#endif

/*
 * XXX TODO: Use autoconf to handle non-FreeBSD platforms.
 *
 * #if !defined(__FreeBSD__)
 *    #include "config.h"
 * #endif
 */

#endif /* !ARCHIVE_H_INCLUDED */
