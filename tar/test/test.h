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
 * $FreeBSD: src/usr.bin/tar/test/test.h,v 1.4 2008/08/21 07:04:57 kientzle Exp $
 */

/* Every test program should #include "test.h" as the first thing. */

/*
 * The goal of this file (and the matching test.c) is to
 * simplify the very repetitive test-*.c test programs.
 */
#if defined(HAVE_CONFIG_H)
/* Most POSIX platforms use the 'configure' script to build config.h */
#include "config.h"
#elif defined(__FreeBSD__)
/* Building as part of FreeBSD system requires a pre-built config.h. */
#include "config_freebsd.h"
#elif defined(_WIN32) && !defined(__CYGWIN__)
/* Win32 can't run the 'configure' script. */
#include "config_windows.h"
#else
/* Warn if the library hasn't been (automatically or manually) configured. */
#error Oops: No config.h and no pre-built configuration in test.h.
#endif

#include <sys/types.h>  /* Windows requires this before sys/stat.h */
#include <sys/stat.h>

#if HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#define dirent direct
#endif
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <wchar.h>
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

/*
 * System-specific tweaks.  We really want to minimize these
 * as much as possible, since they make it harder to understand
 * the mainline code.
 */

/* Windows (including Visual Studio and MinGW but not Cygwin) */
#if defined(_WIN32) && !defined(__CYGWIN__)
#if !defined(__BORLANDC__)
#undef chdir
#define chdir _chdir
#define strdup _strdup
#endif
#endif

/* Visual Studio */
#ifdef _MSC_VER
#define snprintf	sprintf_s
#endif

#if defined(__BORLANDC__)
#pragma warn -8068	/* Constant out of range in comparison. */
#endif

/* Haiku OS and QNX */
#if defined(__HAIKU__) || defined(__QNXNTO__)
/* Haiku and QNX have typedefs in stdint.h (needed for int64_t) */
#include <stdint.h>
#endif

/* Get a real definition for __FBSDID if we can */
#if HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

/* If not, define it so as to avoid dangling semicolons. */
#ifndef __FBSDID
#define	__FBSDID(a)     struct _undefined_hack
#endif

#ifndef O_BINARY
#define	O_BINARY 0
#endif

/*
 * Redefine DEFINE_TEST for use in defining the test functions.
 */
#undef DEFINE_TEST
#define DEFINE_TEST(name) void name(void); void name(void)

/* An implementation of the standard assert() macro */
#define assert(e)   assertion_assert(__FILE__, __LINE__, (e), #e, NULL)
/* chdir() and error if it fails */
#define assertChdir(path)  \
  assertion_chdir(__FILE__, __LINE__, path)
/* Assert two integers are the same.  Reports value of each one if not. */
#define assertEqualInt(v1,v2) \
  assertion_equal_int(__FILE__, __LINE__, (v1), #v1, (v2), #v2, NULL)
/* Assert two strings are the same.  Reports value of each one if not. */
#define assertEqualString(v1,v2)   \
  assertion_equal_string(__FILE__, __LINE__, (v1), #v1, (v2), #v2, NULL, 0)
#define assertEqualUTF8String(v1,v2)   \
  assertion_equal_string(__FILE__, __LINE__, (v1), #v1, (v2), #v2, NULL, 1)
/* As above, but v1 and v2 are wchar_t * */
#define assertEqualWString(v1,v2)   \
  assertion_equal_wstring(__FILE__, __LINE__, (v1), #v1, (v2), #v2, NULL)
/* As above, but raw blocks of bytes. */
#define assertEqualMem(v1, v2, l)	\
  assertion_equal_mem(__FILE__, __LINE__, (v1), #v1, (v2), #v2, (l), #l, NULL)
/* Assert two files are the same. */
#define assertEqualFile(f1, f2)	\
  assertion_equal_file(__FILE__, __LINE__, (f1), (f2))
/* Assert that a file is empty. */
#define assertEmptyFile(pathname)	\
  assertion_empty_file(__FILE__, __LINE__, (pathname))
/* Assert that a file is not empty. */
#define assertNonEmptyFile(pathname)		\
  assertion_non_empty_file(__FILE__, __LINE__, (pathname))
#define assertFileAtime(pathname, sec, nsec)	\
  assertion_file_atime(__FILE__, __LINE__, pathname, sec, nsec)
#define assertFileAtimeRecent(pathname)	\
  assertion_file_atime_recent(__FILE__, __LINE__, pathname)
#define assertFileBirthtime(pathname, sec, nsec)	\
  assertion_file_birthtime(__FILE__, __LINE__, pathname, sec, nsec)
#define assertFileBirthtimeRecent(pathname) \
  assertion_file_birthtime_recent(__FILE__, __LINE__, pathname)
/* Assert that a file exists; supports printf-style arguments. */
#define assertFileExists(pathname) \
  assertion_file_exists(__FILE__, __LINE__, pathname)
/* Assert that a file exists. */
#define assertFileNotExists(pathname) \
  assertion_file_not_exists(__FILE__, __LINE__, pathname)
/* Assert that file contents match a string. */
#define assertFileContents(data, data_size, pathname) \
  assertion_file_contents(__FILE__, __LINE__, data, data_size, pathname)
#define assertFileMtime(pathname, sec, nsec)	\
  assertion_file_mtime(__FILE__, __LINE__, pathname, sec, nsec)
#define assertFileMtimeRecent(pathname) \
  assertion_file_mtime_recent(__FILE__, __LINE__, pathname)
#define assertFileNLinks(pathname, nlinks)  \
  assertion_file_nlinks(__FILE__, __LINE__, pathname, nlinks)
#define assertFileSize(pathname, size)  \
  assertion_file_size(__FILE__, __LINE__, pathname, size)
#define assertTextFileContents(text, pathname) \
  assertion_text_file_contents(__FILE__, __LINE__, text, pathname)
#define assertFileContainsLinesAnyOrder(pathname, lines)	\
  assertion_file_contains_lines_any_order(__FILE__, __LINE__, pathname, lines)
#define assertIsDir(pathname, mode)		\
  assertion_is_dir(__FILE__, __LINE__, pathname, mode)
#define assertIsHardlink(path1, path2)	\
  assertion_is_hardlink(__FILE__, __LINE__, path1, path2)
#define assertIsNotHardlink(path1, path2)	\
  assertion_is_not_hardlink(__FILE__, __LINE__, path1, path2)
#define assertIsReg(pathname, mode)		\
  assertion_is_reg(__FILE__, __LINE__, pathname, mode)
#define assertIsSymlink(pathname, contents)	\
  assertion_is_symlink(__FILE__, __LINE__, pathname, contents)
/* Create a directory, report error if it fails. */
#define assertMakeDir(dirname, mode)	\
  assertion_make_dir(__FILE__, __LINE__, dirname, mode)
#define assertMakeFile(path, mode, contents) \
  assertion_make_file(__FILE__, __LINE__, path, mode, -1, contents)
#define assertMakeBinFile(path, mode, csize, contents) \
  assertion_make_file(__FILE__, __LINE__, path, mode, csize, contents)
#define assertMakeHardlink(newfile, oldfile)	\
  assertion_make_hardlink(__FILE__, __LINE__, newfile, oldfile)
#define assertMakeSymlink(newfile, linkto)	\
  assertion_make_symlink(__FILE__, __LINE__, newfile, linkto)
#define assertNodump(path)      \
  assertion_nodump(__FILE__, __LINE__, path)
#define assertUmask(mask)	\
  assertion_umask(__FILE__, __LINE__, mask)
#define assertUtimes(pathname, atime, atime_nsec, mtime, mtime_nsec)	\
  assertion_utimes(__FILE__, __LINE__, pathname, atime, atime_nsec, mtime, mtime_nsec)

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
int assertion_assert(const char *, int, int, const char *, void *);
int assertion_chdir(const char *, int, const char *);
int assertion_empty_file(const char *, int, const char *);
int assertion_equal_file(const char *, int, const char *, const char *);
int assertion_equal_int(const char *, int, long long, const char *, long long, const char *, void *);
int assertion_equal_mem(const char *, int, const void *, const char *, const void *, const char *, size_t, const char *, void *);
int assertion_equal_string(const char *, int, const char *v1, const char *, const char *v2, const char *, void *, int);
int assertion_equal_wstring(const char *, int, const wchar_t *v1, const char *, const wchar_t *v2, const char *, void *);
int assertion_file_atime(const char *, int, const char *, long, long);
int assertion_file_atime_recent(const char *, int, const char *);
int assertion_file_birthtime(const char *, int, const char *, long, long);
int assertion_file_birthtime_recent(const char *, int, const char *);
int assertion_file_contains_lines_any_order(const char *, int, const char *, const char **);
int assertion_file_contents(const char *, int, const void *, int, const char *);
int assertion_file_exists(const char *, int, const char *);
int assertion_file_mtime(const char *, int, const char *, long, long);
int assertion_file_mtime_recent(const char *, int, const char *);
int assertion_file_nlinks(const char *, int, const char *, int);
int assertion_file_not_exists(const char *, int, const char *);
int assertion_file_size(const char *, int, const char *, long);
int assertion_is_dir(const char *, int, const char *, int);
int assertion_is_hardlink(const char *, int, const char *, const char *);
int assertion_is_not_hardlink(const char *, int, const char *, const char *);
int assertion_is_reg(const char *, int, const char *, int);
int assertion_is_symlink(const char *, int, const char *, const char *);
int assertion_make_dir(const char *, int, const char *, int);
int assertion_make_file(const char *, int, const char *, int, int, const void *);
int assertion_make_hardlink(const char *, int, const char *newpath, const char *);
int assertion_make_symlink(const char *, int, const char *newpath, const char *);
int assertion_nodump(const char *, int, const char *);
int assertion_non_empty_file(const char *, int, const char *);
int assertion_text_file_contents(const char *, int, const char *buff, const char *f);
int assertion_umask(const char *, int, int);
int assertion_utimes(const char *, int, const char *, long, long, long, long );

void skipping_setup(const char *, int);
void test_skipping(const char *fmt, ...);

/* Like sprintf, then system() */
int systemf(const char * fmt, ...);

/* Delay until time() returns a value after this. */
void sleepUntilAfter(time_t);

/* Return true if this platform can create symlinks. */
int canSymlink(void);

/* Return true if this platform can run the "bzip2" program. */
int canBzip2(void);

/* Return true if this platform can run the "grzip" program. */
int canGrzip(void);

/* Return true if this platform can run the "gzip" program. */
int canGzip(void);

/* Return true if this platform can run the "lrzip" program. */
int canLrzip(void);

/* Return true if this platform can run the "lzip" program. */
int canLzip(void);

/* Return true if this platform can run the "lzma" program. */
int canLzma(void);

/* Return true if this platform can run the "lzop" program. */
int canLzop(void);

/* Return true if this platform can run the "xz" program. */
int canXz(void);

/* Return true if this filesystem can handle nodump flags. */
int canNodump(void);

/* Return true if the file has large i-node number(>0xffffffff). */
int is_LargeInode(const char *);

/* Suck file into string allocated via malloc(). Call free() when done. */
/* Supports printf-style args: slurpfile(NULL, "%s/myfile", refdir); */
char *slurpfile(size_t *, const char *fmt, ...);

/* Extracts named reference file to the current directory. */
void extract_reference_file(const char *);

/* Path to working directory for current test */
const char *testworkdir;

/*
 * Special interfaces for program test harness.
 */

/* Pathname of exe to be tested. */
const char *testprogfile;
/* Name of exe to use in printf-formatted command strings. */
/* On Windows, this includes leading/trailing quotes. */
const char *testprog;

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif
