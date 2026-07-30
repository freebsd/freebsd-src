/*-
 * Copyright (c) 2014 Sebastian Freundt
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
 */

#include "test.h"

DEFINE_TEST(test_read_format_warc)
{
	char buff[256U];
	const char reffile[] = "test_read_format_warc.warc";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));

	/* First Entry */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("sometest.txt", archive_entry_pathname(ae));
	assertEqualInt(1402399833, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_uid(ae));
	assertEqualInt(0, archive_entry_gid(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(65, archive_entry_size(ae));
	assertEqualInt(65, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "This is a sample text file for libarchive's WARC reader/writer.\n\n", 65);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	/* Second Entry */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("moretest.txt", archive_entry_pathname(ae));
	assertEqualInt(1402399884, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_uid(ae));
	assertEqualInt(0, archive_entry_gid(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(76, archive_entry_size(ae));
	assertA(76 == archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "The beauty is that WARC remains ASCII only iff all contents are ASCII only.\n", 76);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);

	/* Test EOF */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(2, archive_file_count(a));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_WARC, archive_format(a));

	/* Verify closing and resource freeing */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_warc_incomplete)
{
	const char reffile[] = "test_read_format_warc_incomplete.warc";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));

	/* Entry cannot be parsed */
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));

	/* Verify closing and resource freeing */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_warc_skip_after_extract)
{
#define WARC_APPEND(s) do { \
	memcpy(p, s, sizeof(s) - 1); \
	p += sizeof(s) - 1; \
} while (0)
	const size_t body_size = 200000;
	char *warc;
	char *p;
	char second[6];
	size_t used;
	int fd;
	struct archive_entry *ae;
	struct archive *a;

	warc = malloc(body_size + 1024);
	assert(warc != NULL);
	p = warc;

	WARC_APPEND("WARC/1.0\r\n"
	    "WARC-Type: resource\r\n"
	    "WARC-Date: 2014-06-10T10:10:10Z\r\n"
	    "WARC-Target-URI: file://first.txt\r\n"
	    "Content-Length: 200000\r\n"
	    "\r\n");
	memset(p, 'A', body_size);
	p += body_size;
	WARC_APPEND("\r\n\r\n"
	    "WARC/1.0\r\n"
	    "WARC-Type: resource\r\n"
	    "WARC-Date: 2014-06-10T10:10:11Z\r\n"
	    "WARC-Target-URI: file://second.txt\r\n"
	    "Content-Length: 6\r\n"
	    "\r\n"
	    "SECOND"
	    "\r\n\r\n");
	used = (size_t)(p - warc);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory2(a, warc, used, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("first.txt", archive_entry_pathname(ae));

	fd = open("warc.out", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(fd >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, fd));
	assertEqualInt(0, close(fd));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("second.txt", archive_entry_pathname(ae));
	assertEqualInt(6, archive_entry_size(ae));
	assertEqualInt(6, archive_read_data(a, second, sizeof(second)));
	assertEqualMem(second, "SECOND", 6);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(warc);
#undef WARC_APPEND
}

DEFINE_TEST(test_read_format_warc_truncated_body)
{
	static const char warc[] =
	    "WARC/1.0\r\n"
	    "WARC-Type: resource\r\n"
	    "WARC-Date: 2014-06-10T10:10:10Z\r\n"
	    "WARC-Target-URI: file://test.txt\r\n"
	    "Content-Length: 10\r\n"
	    "\r\n"
	    "ABC";
	char body[3];
	struct archive_entry *ae;
	struct archive *a;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, warc, sizeof(warc) - 1U));

	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualInt(3,
	    archive_read_data(a, body, sizeof(body)));
	assertEqualMem(body, "ABC", sizeof(body));
	assertEqualIntA(a, ARCHIVE_FATAL,
	    archive_read_data(a, body, sizeof(body)));
	assertEqualString("Truncated WARC file data",
	    archive_error_string(a));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
