/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */

/*
 * This header is the first thing included in any of the bsdtar
 * source files.  As far as possible, platform-specific issues should
 * be dealt with here and not within individual source files.
 */

#ifndef BSDCAT_PLATFORM_H_INCLUDED
#define	BSDCAT_PLATFORM_H_INCLUDED

#if defined(PLATFORM_CONFIG_H)
/* Use hand-built config.h in environments that need it. */
#include PLATFORM_CONFIG_H
#else
/* Not having a config.h of some sort is a serious problem. */
#include "config.h"
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

/* How to mark functions that don't return. */
/* This facilitates use of some newer static code analysis tools. */
#undef __LA_NORETURN
#if defined(__GNUC__) && (__GNUC__ > 2 || \
                          (__GNUC__ == 2 && __GNUC_MINOR__ >= 5))
#define __LA_NORETURN       __attribute__((__noreturn__))
#elif defined(_MSC_VER)
#define __LA_NORETURN __declspec(noreturn)
#else
#define __LA_NORETURN
#endif

#endif /* !BSDCAT_PLATFORM_H_INCLUDED */
