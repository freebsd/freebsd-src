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

#if HAVE_CONFIG_H
#include "config.h"
#else

/* A default configuration for FreeBSD, used if there is no config.h. */
#ifdef __FreeBSD__
#if __FreeBSD__ > 4
#define	HAVE_ACL_CREATE_ENTRY 1
#define	HAVE_ACL_INIT 1
#define	HAVE_ACL_SET_FD 1
#define	HAVE_ACL_SET_FD_NP 1
#define	HAVE_ACL_SET_FILE 1
#endif
#define	HAVE_BZLIB_H 1
#define	HAVE_CHFLAGS 1
#define	HAVE_DECL_STRERROR_R 1
#define	HAVE_EFTYPE 1
#define	HAVE_EILSEQ 1
#define	HAVE_ERRNO_H 1
#define	HAVE_FCHDIR 1
#define	HAVE_FCHMOD 1
#define	HAVE_FCHOWN 1
#define	HAVE_FCNTL_H 1
#define	HAVE_FUTIMES 1
#define	HAVE_INTTYPES_H 1
#define	HAVE_LCHMOD 1
#define	HAVE_LCHOWN 1
#define	HAVE_LIMITS_H 1
#define	HAVE_LUTIMES 1
#define	HAVE_MALLOC 1
#define	HAVE_MEMMOVE 1
#define	HAVE_MEMORY_H 1
#define	HAVE_MEMSET 1
#define	HAVE_MKDIR 1
#define	HAVE_MKFIFO 1
#define	HAVE_PATHS_H 1
#define	HAVE_STDINT_H 1
#define	HAVE_STDLIB_H 1
#define	HAVE_STRCHR 1
#define	HAVE_STRDUP 1
#define	HAVE_STRERROR 1
#define	HAVE_STRERROR_R 1
#define	HAVE_STRINGS_H 1
#define	HAVE_STRING_H 1
#define	HAVE_STRRCHR 1
#define	HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC 1
#define	HAVE_STRUCT_STAT_ST_RDEV 1
#define	HAVE_SYS_ACL_H 1
#define	HAVE_SYS_IOCTL_H 1
#define	HAVE_SYS_STAT_H 1
#define	HAVE_SYS_TIME_H 1
#define	HAVE_SYS_TYPES_H 1
#define	HAVE_SYS_WAIT_H 1
#define	HAVE_UNISTD_H 1
#define	HAVE_WCHAR_H 1
#define	HAVE_ZLIB_H 1
#define	STDC_HEADERS 1
#define	TIME_WITH_SYS_TIME 1
#else /* !__FreeBSD__ */
/* Warn if the library hasn't been (automatically or manually) configured. */
#error Oops: No config.h and no built-in configuration in archive_platform.h.
#endif /* !__FreeBSD__ */

#endif /* !HAVE_CONFIG_H */

/* No non-FreeBSD platform will have __FBSDID, so just define it here. */
#ifdef __FreeBSD__
#include <sys/cdefs.h>  /* For __FBSDID */
#else
#define	__FBSDID(a)     /* null */
#endif

#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif

/* FreeBSD 4 and earlier lack intmax_t/uintmax_t */
#if defined(__FreeBSD__) && __FreeBSD__ < 5
#define	intmax_t int64_t
#define	uintmax_t uint64_t
#endif

/*
 * If this platform has <sys/acl.h>, acl_create(), acl_init(), and
 * acl_set_file(), we assume it has the rest of the POSIX.1e draft
 * functions used in archive_read_extract.c.
 */
#if HAVE_SYS_ACL_H && HAVE_ACL_CREATE_ENTRY && HAVE_ACL_INIT && HAVE_ACL_SET_FILE
#define	HAVE_POSIX_ACL	1
#endif

/*
 * If we can't restore metadata using a file descriptor, then
 * for compatibility's sake, close files before trying to restore metadata.
 */
#if defined(HAVE_FCHMOD) || defined(HAVE_FUTIMES) || defined(HAVE_ACL_SET_FD) || defined(HAVE_ACL_SET_FD_NP) || defined(HAVE_FCHOWN)
#define CAN_RESTORE_METADATA_FD
#endif

/* Set up defaults for internal error codes. */
#ifndef ARCHIVE_ERRNO_FILE_FORMAT
#if HAVE_EFTYPE
#define	ARCHIVE_ERRNO_FILE_FORMAT EFTYPE
#else
#if HAVE_EILSEQ
#define	ARCHIVE_ERRNO_FILE_FORMAT EILSEQ
#else
#define	ARCHIVE_ERRNO_FILE_FORMAT EINVAL
#endif
#endif
#endif

#ifndef ARCHIVE_ERRNO_PROGRAMMER
#define	ARCHIVE_ERRNO_PROGRAMMER EINVAL
#endif

#ifndef ARCHIVE_ERRNO_MISC
#define	ARCHIVE_ERRNO_MISC (-1)
#endif

/* Select the best way to set/get hi-res timestamps. */
#if HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
/* FreeBSD uses "timespec" members. */
#define	ARCHIVE_STAT_ATIME_NANOS(st)	(st)->st_atimespec.tv_nsec
#define	ARCHIVE_STAT_CTIME_NANOS(st)	(st)->st_ctimespec.tv_nsec
#define	ARCHIVE_STAT_MTIME_NANOS(st)	(st)->st_mtimespec.tv_nsec
#define	ARCHIVE_STAT_SET_ATIME_NANOS(st, n) (st)->st_atimespec.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_CTIME_NANOS(st, n) (st)->st_ctimespec.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_MTIME_NANOS(st, n) (st)->st_mtimespec.tv_nsec = (n)
#else
#if HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
/* Linux uses "tim" members. */
#define	ARCHIVE_STAT_ATIME_NANOS(pstat)	(pstat)->st_atim.tv_nsec
#define	ARCHIVE_STAT_CTIME_NANOS(pstat)	(pstat)->st_ctim.tv_nsec
#define	ARCHIVE_STAT_MTIME_NANOS(pstat)	(pstat)->st_mtim.tv_nsec
#define	ARCHIVE_STAT_SET_ATIME_NANOS(st, n) (st)->st_atim.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_CTIME_NANOS(st, n) (st)->st_ctim.tv_nsec = (n)
#define	ARCHIVE_STAT_SET_MTIME_NANOS(st, n) (st)->st_mtim.tv_nsec = (n)
#else
/* If we can't find a better way, just use stubs. */
#define	ARCHIVE_STAT_ATIME_NANOS(pstat)	0
#define	ARCHIVE_STAT_CTIME_NANOS(pstat)	0
#define	ARCHIVE_STAT_MTIME_NANOS(pstat)	0
#define	ARCHIVE_STAT_SET_ATIME_NANOS(st, n)
#define	ARCHIVE_STAT_SET_CTIME_NANOS(st, n)
#define	ARCHIVE_STAT_SET_MTIME_NANOS(st, n)
#endif
#endif

#endif /* !ARCHIVE_H_INCLUDED */
