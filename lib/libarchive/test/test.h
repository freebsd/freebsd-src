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
 * $FreeBSD$
 */

/* Every test program should #include "test.h" as the first thing. */

/*
 * The goal of this file (and the matching test.c) is to
 * simplify the very repetitive test-*.c test programs.
 */
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
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
 * Redefine DEFINE_TEST for use in defining the test functions.
 */
#undef DEFINE_TEST
#define DEFINE_TEST(name) void name(void); void name(void)

/* An implementation of the standard assert() macro */
#define assert(e)   test_assert(__FILE__, __LINE__, (e), #e, NULL)

/* Assert two integers are the same.  Reports value of each one if not. */
#define assertEqualInt(v1,v2)   \
  test_assert_equal_int(__FILE__, __LINE__, (v1), #v1, (v2), #v2, NULL)

/* Assert two strings are the same.  Reports value of each one if not. */
#define assertEqualString(v1,v2)   \
  test_assert_equal_string(__FILE__, __LINE__, (v1), #v1, (v2), #v2, NULL)
/* As above, but v1 and v2 are wchar_t * */
#define assertEqualWString(v1,v2)   \
  test_assert_equal_wstring(__FILE__, __LINE__, (v1), #v1, (v2), #v2, NULL)
/* As above, but raw blocks of bytes. */
#define assertEqualMem(v1, v2, l)	\
  test_assert_equal_mem(__FILE__, __LINE__, (v1), #v1, (v2), #v2, (l), #l, NULL)
/* Assert two files are the same; allow printf-style expansion of second name.
 * See below for comments about variable arguments here...
 */
#define assertEqualFile		\
  test_setup(__FILE__, __LINE__);test_assert_equal_file
/* Assert that a file is empty; supports printf-style arguments. */
#define assertEmptyFile		\
  test_setup(__FILE__, __LINE__);test_assert_empty_file

/*
 * This would be simple with C99 variadic macros, but I don't want to
 * require that.  Instead, I insert a function call before each
 * skipping() call to pass the file and line information down.  Crude,
 * but effective.
 */
#define skipping	\
  test_setup(__FILE__, __LINE__);test_skipping

/* Function declarations.  These are defined in test_utility.c. */
void failure(const char *fmt, ...);
void test_setup(const char *, int);
void test_skipping(const char *fmt, ...);
int test_assert(const char *, int, int, const char *, void *);
void test_assert_empty_file(const char *, ...);
void test_assert_equal_file(const char *, const char *, ...);
void test_assert_equal_int(const char *, int, int, const char *, int, const char *, void *);
void test_assert_equal_string(const char *, int, const char *v1, const char *, const char *v2, const char *, void *);
void test_assert_equal_wstring(const char *, int, const wchar_t *v1, const char *, const wchar_t *v2, const char *, void *);
void test_assert_equal_mem(const char *, int, const char *, const char *, const char *, const char *, size_t, const char *, void *);

/* Like sprintf, then system() */
int systemf(const char * fmt, ...);

/* Suck file into string allocated via malloc(). Call free() when done. */
/* Supports printf-style args: slurpfile(NULL, "%s/myfile", refdir); */
char *slurpfile(size_t *, const char *fmt, ...);

/*
 * Global vars
 */

/* Directory holding reference files. */
char *refdir;

/*
 * Special interfaces for libarchive test harness.
 */

#include "archive.h"
#include "archive_entry.h"

/* Special customized read-from-memory interface. */
int read_open_memory(struct archive *, void *, size_t, size_t);

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

/* Versions of above that accept an archive argument for additional info. */
#define assertA(e)   test_assert(__FILE__, __LINE__, (e), #e, (a))
#define assertEqualIntA(a,v1,v2)   \
  test_assert_equal_int(__FILE__, __LINE__, (v1), #v1, (v2), #v2, (a))
#define assertEqualStringA(a,v1,v2)   \
  test_assert_equal_string(__FILE__, __LINE__, (v1), #v1, (v2), #v2, (a))
