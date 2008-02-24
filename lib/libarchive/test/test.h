/*
 * Copyright (c) 2003-2006 Tim Kientzle
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
 * $FreeBSD: src/lib/libarchive/test/test.h,v 1.6 2007/07/14 17:52:01 kientzle Exp $
 */

/* Every test program should #include "test.h" as the first thing. */

/*
 * The goal of this file (and the matching test.c) is to
 * simplify the very repetitive test-*.c test programs.
 */

#define _FILE_OFFSET_BITS 64

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#if defined(HAVE_CONFIG_H)
/* Most POSIX platforms use the 'configure' script to build config.h */
#include "../../config.h"
#elif defined(__FreeBSD__)
/* Building as part of FreeBSD system requires a pre-built config.h. */
#include "../config_freebsd.h"
#elif defined(_WIN32)
/* Win32 can't run the 'configure' script. */
#include "../config_windows.h"
#else
/* Warn if the library hasn't been (automatically or manually) configured. */
#error Oops: No config.h and no pre-built configuration in test.h.
#endif

/* No non-FreeBSD platform will have __FBSDID, so just define it here. */
#ifdef __FreeBSD__
#include <sys/cdefs.h>  /* For __FBSDID */
#else
#define	__FBSDID(a)     /* null */
#endif

/*
 * ARCHIVE_VERSION_STAMP first appeared in 1.9 and libarchive 2.2.4.
 * We can approximate it for earlier versions, though.
 * This is used to disable tests of features not present in the current
 * version.
 */
#ifndef ARCHIVE_VERSION_STAMP
#define ARCHIVE_VERSION_STAMP	\
		(ARCHIVE_API_VERSION * 1000000 + ARCHIVE_API_FEATURE * 1000)
#endif


/*
 * "list.h" is simply created by "grep DEFINE_TEST"; it has
 * a line like
 *      DEFINE_TEST(test_function)
 * for each test.
 * Include it here with a suitable DEFINE_TEST to declare all of the
 * test functions.
 */
#define DEFINE_TEST(name) void name(void);
#include "list.h"
/*
 * Redefine DEFINE_TEST for use in defining the test functions.
 */
#undef DEFINE_TEST
#define DEFINE_TEST(name) void name(void)

/* An implementation of the standard assert() macro */
#define assert(e)   test_assert(__FILE__, __LINE__, (e), #e, NULL)
/* As above, but reports any archive_error found in variable 'a' */
#define assertA(e)   test_assert(__FILE__, __LINE__, (e), #e, (a))

/* Asserts that two integers are the same.  Reports value of each one if not. */
#define assertEqualIntA(a,v1,v2)   \
  test_assert_equal_int(__FILE__, __LINE__, (v1), #v1, (v2), #v2, (a))
#define assertEqualInt(v1,v2)   \
  test_assert_equal_int(__FILE__, __LINE__, (v1), #v1, (v2), #v2, NULL)

/* Asserts that two strings are the same.  Reports value of each one if not. */
#define assertEqualStringA(a,v1,v2)   \
  test_assert_equal_string(__FILE__, __LINE__, (v1), #v1, (v2), #v2, (a))
#define assertEqualString(v1,v2)   \
  test_assert_equal_string(__FILE__, __LINE__, (v1), #v1, (v2), #v2, NULL)
/* As above, but v1 and v2 are wchar_t * */
#define assertEqualWString(v1,v2)   \
  test_assert_equal_wstring(__FILE__, __LINE__, (v1), #v1, (v2), #v2, NULL)

/*
 * This would be simple with C99 variadic macros, but I don't want to
 * require that.  Instead, I insert a function call before each
 * skipping() call to pass the file and line information down.  Crude,
 * but effective.
 */
#define skipping	\
  skipping_setup(__FILE__, __LINE__);test_skipping

/* Function declarations.  These are defined in test_utility.c. */
void failure(const char *fmt, ...);
void skipping_setup(const char *, int);
void test_skipping(const char *fmt, ...);
void test_assert(const char *, int, int, const char *, struct archive *);
void test_assert_equal_int(const char *, int, int, const char *, int, const char *, struct archive *);
void test_assert_equal_string(const char *, int, const char *v1, const char *, const char *v2, const char *, struct archive *);
void test_assert_equal_wstring(const char *, int, const wchar_t *v1, const char *, const wchar_t *v2, const char *, struct archive *);

/* Special customized read-from-memory interface. */
int read_open_memory(struct archive *, void *, size_t, size_t);
int	read_open_memory(struct archive *, void *, size_t, size_t);
