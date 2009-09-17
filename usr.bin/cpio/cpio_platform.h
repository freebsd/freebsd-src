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
 * $FreeBSD$
 */

/*
 * This header is the first thing included in any of the cpio
 * source files.  As far as possible, platform-specific issues should
 * be dealt with here and not within individual source files.
 */

#ifndef CPIO_PLATFORM_H_INCLUDED
#define	CPIO_PLATFORM_H_INCLUDED

#if defined(PLATFORM_CONFIG_H)
/* Use hand-built config.h in environments that need it. */
#include PLATFORM_CONFIG_H
#elif defined(HAVE_CONFIG_H)
/* Most POSIX platforms use the 'configure' script to build config.h */
#include "../config.h"
#else
/* Warn if cpio hasn't been (automatically or manually) configured. */
#error Oops: No config.h and no built-in configuration in cpio_platform.h.
#endif /* !HAVE_CONFIG_H */

/* No non-FreeBSD platform will have __FBSDID, so just define it here. */
#ifdef __FreeBSD__
#include <sys/cdefs.h>  /* For __FBSDID */
#else
/* Just leaving this macro replacement empty leads to a dangling semicolon. */
#define	__FBSDID(a)     struct _undefined_hack
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
 * We need to be able to display a filesize using printf().  The type
 * and format string here must be compatible with one another and
 * large enough for any file.
 */
#if HAVE_UINTMAX_T
#define	CPIO_FILESIZE_TYPE	uintmax_t
#define	CPIO_FILESIZE_PRINTF	"%ju"
#else
#if HAVE_UNSIGNED_LONG_LONG
#define	CPIO_FILESIZE_TYPE	unsigned long long
#define	CPIO_FILESIZE_PRINTF	"%llu"
#else
#define	CPIO_FILESIZE_TYPE	unsigned long
#define	CPIO_FILESIZE_PRINTF	"%lu"
#endif
#endif

/* How to mark functions that don't return. */
#if defined(__GNUC__) && (__GNUC__ > 2 || \
                          (__GNUC__ == 2 && __GNUC_MINOR__ >= 5))
#define __LA_DEAD       __attribute__((__noreturn__))
#else
#define __LA_DEAD
#endif

#endif /* !CPIO_PLATFORM_H_INCLUDED */
