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
 * be dealt with here and not within individual source files.
 */

#ifndef ARCHIVE_PLATFORM_H_INCLUDED
#define	ARCHIVE_PLATFORM_H_INCLUDED

/* FreeBSD-specific definitions. */
#ifdef __FreeBSD__
#include <sys/cdefs.h>  /* For __FBSDID */
#define HAVE_POSIX_ACL 1
#define HAVE_CHFLAGS 1
#define HAVE_LUTIMES 1
#define HAVE_LCHMOD 1
#define ARCHIVE_ERRNO_FILE_FORMAT EFTYPE
#define ARCHIVE_ERRNO_PROGRAMMER EDOOFUS
#define ARCHIVE_ERRNO_MISC (-1)
#endif

/* No non-FreeBSD platform will have __FBSDID, so just define it here. */
#ifndef __FreeBSD__
#define __FBSDID(a)     /* null */
#endif

/* Linux */
#ifdef LINUX
#define ARCHIVE_ERRNO_FILE_FORMAT EILSEQ
#define ARCHIVE_ERRNO_PROGRAMMER EINVAL
#define ARCHIVE_ERRNO_MISC (-1)
#define st_atimespec st_atim
#define st_mtimespec st_mtim
#define st_ctimespec st_ctim
#endif

/*
 * XXX TODO: Use autoconf to handle non-FreeBSD platforms.
 *
 * #if !defined(__FreeBSD__)
 *    #include "config.h"
 * #endif
 */

#endif /* !ARCHIVE_H_INCLUDED */
