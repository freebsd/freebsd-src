/*-
 * Copyright (c) 2023 Martin Matuska
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

DEFINE_TEST(test_read_filter_uudecode_raw)
{
	struct archive_entry *ae;
	struct archive *a;
	char buf[1024];

	const char *name = "test_read_filter_uudecode_raw.uu";

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	copy_reference_file(name);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, name, 670));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("LICENSE.txt", archive_entry_pathname(ae));
	assertEqualInt((AE_IFREG | 0755), archive_entry_mode(ae));
	assertEqualIntA(a, 465, archive_read_data(a, buf, sizeof(buf)));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_filter_uudecode_base64_raw)
{
	struct archive_entry *ae;
	struct archive *a;

	const char *name = "test_read_filter_uudecode_base64_raw.uu";

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	copy_reference_file(name);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, name, 670));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("LICENSE2.txt", archive_entry_pathname(ae));
	assertEqualInt((AE_IFREG | 0600), archive_entry_mode(ae));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Reads the given data, which should be uuencoded or base64 encoded
 * "hi\n", with the uudecode filter and the raw format. If the filter is
 * expected to recognize it, the entry has the given name and mode.
 */
static void
check_header(const char *data, const char *want_name, int want_mode)
{
	struct archive_entry *ae;
	struct archive *a;
	char buf[16];

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, data, strlen(data)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	if (want_name != NULL) {
		assertEqualInt(ARCHIVE_FILTER_UU, archive_filter_code(a, 0));
		assertEqualString(want_name, archive_entry_pathname(ae));
		assertEqualInt((AE_IFREG | want_mode), archive_entry_mode(ae));
		assertEqualIntA(a, 3, archive_read_data(a, buf, sizeof(buf)));
		assertEqualMem("hi\n", buf, 3);
	} else {
		/* Not a header: the data is read as it is. */
		assertEqualInt(ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * The mode in the "begin" line is a sequence of octal digits. Encoders
 * don't all write three of them, for example 0644 instead of 644.
 */
DEFINE_TEST(test_read_filter_uudecode_mode_digits)
{
	/* The usual three digits. */
	check_header("begin 644 test.txt\n#:&D*\n`\nend\n", "test.txt", 0644);

	/* Four digits, with a leading zero. */
	check_header("begin 0744 test.txt\n#:&D*\n`\nend\n", "test.txt", 0744);

	/* Four digits: only the permission bits are kept. */
	check_header("begin 4755 test.txt\n#:&D*\n`\nend\n", "test.txt", 0755);

	/* Same for the base64 encoding. */
	check_header("begin-base64 600 test.txt\naGkK\n====\n",
	    "test.txt", 0600);
	check_header("begin-base64 0600 test.txt\naGkK\n====\n",
	    "test.txt", 0600);
}

/* Lines that are not a valid header are not recognized. */
DEFINE_TEST(test_read_filter_uudecode_invalid_mode)
{
	/* A character that is not an octal digit. */
	check_header("begin 64x test.txt\n#:&D*\n`\nend\n", NULL, 0);
	check_header("begin 648 test.txt\n#:&D*\n`\nend\n", NULL, 0);

	/* Fewer than three digits. */
	check_header("begin 44 test.txt\n#:&D*\n`\nend\n", NULL, 0);
	check_header("begin-base64 6 test.txt\naGkK\n====\n", NULL, 0);

	/* No digit. */
	check_header("begin  test.txt\n#:&D*\n`\nend\n", NULL, 0);

	/* No space after the mode. */
	check_header("begin 644test.txt\n#:&D*\n`\nend\n", NULL, 0);

	/* No name. */
	check_header("begin 644 \n#:&D*\n`\nend\n", NULL, 0);
}
