/*-
 * Copyright (c) 2024 Yang Zhou
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

static void
test_with_hdrcharset(const char *charset)
{
	static const char *raw_path = "dir_stored\\dir1/file";
	static const char *replaced = "dir_stored/dir1/file";
	struct archive *a;
	size_t used;
	size_t buffsize = 1000000;
	char *buff;

	buff = malloc(buffsize);

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	if (charset != NULL) {
		assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_option(a, "zip", "hdrcharset", charset));
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, buffsize, &used));

	/*
	 * Write a file with mixed '/' and '\'
	 */
	struct archive_entry *ae;
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 10);
	archive_entry_copy_pathname(ae, raw_path);
	archive_entry_set_mode(ae, AE_IFREG | 0755);
	archive_entry_set_size(ae, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));
	dumpfile("constructed.zip", buff, used);

	/*
	 * Check if the generated archive contains and only contains expected path.
	 * Intentionally avoid using `archive_read_XXX` functions because it silently replaces '\' with '/',
	 * making it difficult to get the exact path written in the archive.
	 */
#if defined(_WIN32) && !defined(__CYGWIN__)
	const char *expected = replaced;
	const char *unexpected = raw_path;
#else
	const char *expected = raw_path;
	const char *unexpected = replaced;
#endif
	int expected_found = 0;
	int unexpected_found = 0;
	size_t len = strlen(raw_path);
	for (char *ptr = buff; ptr < (buff + used - len); ptr++) {
		if (memcmp(ptr, expected, len) == 0)
			++expected_found;
		if (memcmp(ptr, unexpected, len) == 0)
			++unexpected_found;
	}
	failure("should find expected path in both local and central header (charset=%s)", charset);
	assertEqualInt(2, expected_found);
	failure("should not find unexpected path in anywhere (charset=%s)", charset);
	assertEqualInt(0, unexpected_found);
}

DEFINE_TEST(test_write_format_zip_windows_path)
{
	test_with_hdrcharset(NULL);
#if defined(_WIN32) && !defined(__CYGWIN__) || HAVE_ICONV
	test_with_hdrcharset("ISO-8859-1");
	test_with_hdrcharset("UTF-8");
#endif
}
