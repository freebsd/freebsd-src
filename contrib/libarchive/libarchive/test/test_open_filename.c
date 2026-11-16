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

#include <locale.h>

static void
test_open_filename_mbs(void)
{
	char buff[64];
	struct archive_entry *ae;
	struct archive *a;

	/* Write an archive through this FILE *. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_filename(a, "test.tar"));

	/*
	 * Write a file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 0);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, 8);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 8, archive_write_data(a, "12345678", 9));

	/*
	 * Write a second file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file2");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, 819200);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Close out the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/*
	 * Now, read the data back.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, "test.tar", 512));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assert((S_IFREG | 0755) == archive_entry_mode(ae));
	assertEqualInt(8, archive_entry_size(ae));
	assertEqualIntA(a, 8, archive_read_data(a, buff, 10));
	assertEqualMem(buff, "12345678", 8);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assert((S_IFREG | 0755) == archive_entry_mode(ae));
	assertEqualInt(819200, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_skip(a));

	/* Verify the end of the archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * Verify some of the error handling.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_FATAL,
	    archive_read_open_filename(a, "nonexistent.tar", 512));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

}

static void
test_open_filename_wcs(void)
{
	char buff[64];
	struct archive_entry *ae;
	struct archive *a;

	/* Write an archive through this FILE *. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_filename_w(a, L"test.tar"));

	/*
	 * Write a file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 0);
	archive_entry_copy_pathname_w(ae, L"file");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, 8);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 8, archive_write_data(a, "12345678", 9));

	/*
	 * Write a second file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname_w(ae, L"file2");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, 819200);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Close out the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/*
	 * Now, read the data back.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename_w(a, L"test.tar", 512));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_mtime_nsec(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualWString(L"file", archive_entry_pathname_w(ae));
	assert((S_IFREG | 0755) == archive_entry_mode(ae));
	assertEqualInt(8, archive_entry_size(ae));
	assertEqualIntA(a, 8, archive_read_data(a, buff, 10));
	assertEqualMem(buff, "12345678", 8);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualWString(L"file2", archive_entry_pathname_w(ae));
	assert((S_IFREG | 0755) == archive_entry_mode(ae));
	assertEqualInt(819200, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_skip(a));

	/* Verify the end of the archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * Verify some of the error handling.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_FATAL,
	    archive_read_open_filename_w(a, L"nonexistent.tar", 512));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

}

DEFINE_TEST(test_open_filename)
{
	test_open_filename_mbs();
	test_open_filename_wcs();
}

DEFINE_TEST(test_open_filename_wcs_oob)
{
	struct archive *a;

	/* Continue even if not available for best test coverage. */
	setlocale(LC_ALL, "en_US.UTF-8");

	/*
	 * Try to open file with filename which might use more bytes in
	 * MBS representation than in wchar_t if platform uses 16 bit wchar_t,
	 * e.g. Cygwin (3 times a Euro sign, i.e. 3*2+2=8 vs 3*3+1=10 bytes).
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_FATAL,
	    archive_read_open_filename_w(a, L"\u20AC\u20AC\u20AC", 512));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_open_filename_stdin_zip)
{
#if HAVE_PIPE && (!defined(_WIN32) || defined(__CYGWIN__))
	struct archive *a, *w;
	struct archive_entry *entry;
	char buff[4096];
	size_t used = 0;
	int fd[2], saved_stdin, header_result;

	w = archive_write_new();
	assertEqualIntA(w, ARCHIVE_OK, archive_write_set_format_zip(w));
	assertEqualIntA(w, ARCHIVE_OK,
	    archive_write_set_options(w, "zip:compression=store"));
	assertEqualIntA(w, ARCHIVE_OK, archive_write_add_filter_none(w));
	assertEqualIntA(w, ARCHIVE_OK,
	    archive_write_open_memory(w, buff, sizeof(buff), &used));
	entry = archive_entry_new();
	archive_entry_set_pathname(entry, "file");
	archive_entry_set_mode(entry, AE_IFREG | 0644);
	archive_entry_set_size(entry, 4);
	assertEqualIntA(w, ARCHIVE_OK, archive_write_header(w, entry));
	assertEqualIntA(w, 4, archive_write_data(w, "data", 4));
	archive_entry_free(entry);
	assertEqualInt(ARCHIVE_OK, archive_write_free(w));

	saved_stdin = dup(0);
	if (saved_stdin < 0) {
		skipping("stdin is unavailable");
		return;
	}
	assertEqualInt(0, pipe(fd));
	assertEqualInt((ssize_t)used, write(fd[1], buff, used));
	assertEqualInt(0, close(fd[1]));
	assertEqualInt(0, dup2(fd[0], 0));
	assertEqualInt(0, close(fd[0]));

	a = archive_read_new();
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, NULL, 10240));
	header_result = archive_read_next_header(a, &entry);
	assertEqualIntA(a, ARCHIVE_OK, header_result);
	if (header_result == ARCHIVE_OK)
		assertEqualString("file", archive_entry_pathname(entry));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assertEqualInt(0, dup2(saved_stdin, 0));
	assertEqualInt(0, close(saved_stdin));
#else
	skipping("pipe and dup2 are unavailable on this platform");
#endif
}

DEFINE_TEST(test_open_filename_proc_einval)
{
#if defined(__linux__)
	struct archive *a;
	struct archive_entry *entry;
	char byte;
	int fd;

	fd = open("/proc/version", O_RDONLY);
	if (fd < 0) {
		skipping("/proc/version is unavailable");
		return;
	}

	/* This readable file rejects SEEK_END without moving. */
	if (read(fd, &byte, 1) != 1 ||
	    lseek(fd, 0, SEEK_END) != (off_t)-1 ||
	    errno != EINVAL ||
	    lseek(fd, 0, SEEK_CUR) != 1) {
		close(fd);
		skipping("/proc/version does not reject SEEK_END with EINVAL");
		return;
	}
	close(fd);

	a = archive_read_new();
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, "/proc/version", 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualInt(ARCHIVE_FORMAT_RAW, archive_format(a));
	assertEqualIntA(a, 1, archive_read_data(a, &byte, 1));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
#else
	skipping("requires /proc/version");
#endif
}
