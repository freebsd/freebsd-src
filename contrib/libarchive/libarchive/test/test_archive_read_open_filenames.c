/*-
 * Copyright (c) 2026 Tobias Stoeckmann
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

DEFINE_TEST(test_archive_read_open_filenames_null)
{
	const char *filenames[] = {
		"zero",
		"nonexisting",
		NULL
	};
	char *buf;
	struct archive *a;
	struct archive_entry *ae;

	/* Create a file sufficiently large for bidders to not reach EOF. */
	buf = calloc(512, 1024);
	assert(buf != NULL);
	dumpfile("zero", buf, 512 * 1024);
	free(buf);

	/* Open file and a non-existing one as well. */
	a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	/* Verify that archive can be opened. */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filenames(a, filenames, 4096));

	/* Verify that no entry is found. */
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_archive_read_open_filenames_split_uaf)
{
	const char *reffiles[] = {
		"test_archive_read_open_filenames_split_uaf_1.tar",
		"test_archive_read_open_filenames_split_uaf_2.tar",
		NULL
	};
	struct archive *a;
	struct archive_entry *ae;

	/* Prepare formats and filters for sufficiently large bid requests. */
	a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));

	extract_reference_files(reffiles);

	/* Verify that archive can be opened. */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filenames(a, reffiles, 4096));

	/* Verify that an entry is found. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	/* Verify that no further entry can be found. */
	assertEqualIntA(a, ARCHIVE_RETRY, archive_read_next_header(a, &ae));

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
