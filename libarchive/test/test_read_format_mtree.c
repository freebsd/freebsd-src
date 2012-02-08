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
 */
#include "test.h"
__FBSDID("$FreeBSD: head/lib/libarchive/test/test_read_format_mtree.c 201247 2009-12-30 05:59:21Z kientzle $");

static void
test_read_format_mtree1(void)
{
	const char reffile[] = "test_read_format_mtree.mtree";
	char buff[16];
	struct archive_entry *ae;
	struct archive *a;
	FILE *f;
	/* Compute max 64-bit signed twos-complement value
	 * without relying on overflow.  This assumes that long long
	 * is at least 64 bits. */
	const static long long max_int64 = ((((long long)1) << 62) - 1) + (((long long)1) << 62);
	time_t min_time, t;

	extract_reference_file(reffile);

	/*
	 * An access error occurred on some platform when mtree
	 * format handling open a directory. It is for through
	 * the routine which open a directory that we create
	 * "dir" and "dir2" directories.
	 */
	assertMakeDir("dir", 0775);
	assertMakeDir("dir2", 0775);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_file(a, reffile, 11));

	/*
	 * Read "file", whose data is available on disk.
	 */
	f = fopen("file", "wb");
	assert(f != NULL);
	assertEqualInt(3, fwrite("hi\n", 1, 3, f));
	fclose(f);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_MTREE);
	assertEqualString(archive_entry_pathname(ae), "file");
	assertEqualInt(archive_entry_uid(ae), 18);
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0123);
	assertEqualInt(archive_entry_size(ae), 3);
	assertEqualInt(3, archive_read_data(a, buff, 3));
	assertEqualMem(buff, "hi\n", 3);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir");
	assertEqualInt(AE_IFDIR, archive_entry_filetype(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir/file with space");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "file with space");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3a");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3a/indir3a");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/fullindir2");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/indir2");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3b");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3b/indir3b");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "notindir");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/emptyfile");
	assertEqualInt(archive_entry_size(ae), 0);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/smallfile");
	assertEqualInt(archive_entry_size(ae), 1);

	/* TODO: Mtree reader should probably return ARCHIVE_WARN for this. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/toosmallfile");
	assertEqualInt(archive_entry_size(ae), -1);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/bigfile");
	assertEqualInt(archive_entry_size(ae), max_int64);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/toobigfile");
	/* Size in mtree is max_int64 + 1; should return max_int64. */
	assertEqualInt(archive_entry_size(ae), max_int64);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/veryoldfile");
	/* The value in the file is MIN_INT64_T, but time_t may be narrower. */
	/* Verify min_time is the smallest possible time_t. */
	min_time = archive_entry_mtime(ae);
	assert(min_time <= 0);
	/* Simply asserting min_time - 1 > 0 breaks with some compiler optimizations. */
	t = min_time - 1;
	assert(t > 0);

	/* toooldfile is 1 sec older, which should overflow and get returned
	 * with the same value. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/toooldfile");
	assertEqualInt(archive_entry_mtime(ae), min_time);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(19, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_read_format_mtree2(void)
{
	static char archive[] =
	    "#mtree\n"
	    "d type=dir content=.\n";
	struct archive_entry *ae;
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive, sizeof(archive)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_MTREE);
	assertEqualString(archive_entry_pathname(ae), "d");
	assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Reported to libarchive.googlecode.com as Issue 121.
 */
static void
test_read_format_mtree3(void)
{
	static char archive[] =
	    "#mtree\n"
	    "a type=file contents=file\n"
	    "b type=link link=a\n"
	    "c type=file contents=file\n";
	struct archive_entry *ae;
	struct archive *a;

	assertMakeDir("mtree3", 0777);
	assertChdir("mtree3");
	assertMakeFile("file", 0644, "file contents");

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive, sizeof(archive)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "a");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "b");
	assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "c");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(3, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	assertChdir("..");
}


static void
test_read_format_mtree4(void)
{
	const char reffile[] = "test_read_format_mtree_nomagic.mtree";
	char buff[16];
	struct archive_entry *ae;
	struct archive *a;
	FILE *f;

	assertMakeDir("mtree4", 0777);
	assertChdir("mtree4");

	extract_reference_file(reffile);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_file(a, reffile, 11));

	/*
	 * Read "file", whose data is available on disk.
	 */
	f = fopen("file", "wb");
	assert(f != NULL);
	assertEqualInt(3, fwrite("hi\n", 1, 3, f));
	fclose(f);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_MTREE);
	assertEqualString(archive_entry_pathname(ae), "file");
	assertEqualInt(archive_entry_uid(ae), 18);
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0123);
	assertEqualInt(archive_entry_size(ae), 3);
	assertEqualInt(3, archive_read_data(a, buff, 3));
	assertEqualMem(buff, "hi\n", 3);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir");
	assertEqualInt(AE_IFDIR, archive_entry_filetype(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir/file with space");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "file with space");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3a");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3a/indir3a");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/fullindir2");
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/indir2");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3b");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "dir2/dir3b/indir3b");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "notindir");

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(12, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	assertChdir("..");
}

/*
 * We should get a warning if the contents file doesn't exist.
 */
static void
test_read_format_mtree5(void)
{
	static char archive[] =
	    "#mtree\n"
	    "a type=file contents=nonexistent_file\n";
	struct archive_entry *ae;
	struct archive *a;

	assertMakeDir("mtree5", 0777);
	assertChdir("mtree5");

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive, sizeof(archive)));
	assertEqualIntA(a, ARCHIVE_WARN, archive_read_next_header(a, &ae));
	assert(strlen(archive_error_string(a)) > 0);
	assertEqualString(archive_entry_pathname(ae), "a");
	assertEqualInt(archive_entry_filetype(ae), AE_IFREG);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_file_count(a));
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	assertChdir("..");
}

DEFINE_TEST(test_read_format_mtree)
{
	test_read_format_mtree1();
	test_read_format_mtree2();
	test_read_format_mtree3();
	test_read_format_mtree4();
	test_read_format_mtree5();
}
