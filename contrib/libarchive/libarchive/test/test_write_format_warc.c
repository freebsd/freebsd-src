/*-
 * Copyright (C) 2014 Sebastian Freundt
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

static void test_read(struct archive *a, char *buff, size_t used, char *filedata)
{
	struct archive_entry *ae;
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualIntA(a, 9, archive_read_data(a, filedata, 10));
	assertEqualMem(filedata, "12345678", 9);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_write_format_warc)
{
	char filedata[64];
	struct archive *a;
	struct archive_entry *ae;
	const size_t bsiz = 2048U;
	char *buff;
	size_t used;

	buff = malloc(bsiz);

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_warc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, bsiz, &used));

	/*
	 * Write a file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, 9);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 9, archive_write_data(a, "12345678", 9));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/*
	 * Read from it.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_warc(a));
	test_read(a, buff, used, filedata);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_by_code(a, ARCHIVE_FORMAT_WARC));
	test_read(a, buff, used, filedata);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_set_format(a, ARCHIVE_FORMAT_WARC));
	test_read(a, buff, used, filedata);

	/* Create a new archive */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_warc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
		archive_write_open_memory(a, buff, bsiz, &used));

	/* write first file: that should succeed */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, 9);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 9, archive_write_data(a, "12345678", 9));

	/* write second file: should succeed as well */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test2");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, 9);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 9, archive_write_data(a, "12345678", 9));
	assertEqualIntA(a, 0, archive_write_data(a, "overflow", 8));

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Create a new archive */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_warc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
		archive_write_open_memory(a, buff, bsiz, &used));

	/* write a directory: this should fail */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_filetype(ae, AE_IFDIR);
	archive_entry_set_size(ae, 512);
	assertEqualIntA(a, ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* test whether last archive is indeed empty */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));

	/* Test EOF */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
}

DEFINE_TEST(test_write_format_warc_size)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[2048];
	size_t used;

	memset(buff, 0, sizeof(buff));
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_warc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test");
	archive_entry_set_filetype(ae, AE_IFREG);
	assertEqualIntA(a, ARCHIVE_FAILED, archive_write_header(a, ae));
	assertEqualString("Size required", archive_error_string(a));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}

DEFINE_TEST(test_write_format_warc_incomplete)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[2048];
	size_t used;

	memset(buff, 0, sizeof(buff));
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_warc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, 9);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, 8, archive_write_data(a, "12345678", 8));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_write_close(a));
	assertEqualString("WARC entry is shorter than Content-Length",
	    archive_error_string(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}

DEFINE_TEST(test_write_format_warc_warcinfo_output_error)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[1];
	size_t used;

	memset(buff, 0, sizeof(buff));
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_warc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_per_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, 1);
	assertEqualIntA(a, ARCHIVE_FATAL, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}

DEFINE_TEST(test_write_format_warc_resource_output_error)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[1];
	size_t used;

	memset(buff, 0, sizeof(buff));
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_warc(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_format_option(a, NULL, "omit-warcinfo", NULL));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_per_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "test");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, 1);
	assertEqualIntA(a, ARCHIVE_FATAL, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}
