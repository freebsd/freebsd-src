/*-
 * Copyright (c) 2026 krishna28238-arch
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

/*
 * Regression test for skipping files across CFFOLDER boundaries.
 *
 * The test archive holds two folders with one uncompressed file each:
 *   - file "a" in folder 0, containing 20 bytes of 'A'
 *   - file "b" in folder 1, containing the 40 bytes 00 01 02 ... 27
 *
 * When "a" is skipped without reading, the skipped byte count must not
 * be charged against folder 1's data when "b" is read afterwards.  With
 * the old code, reading "b" after skipping "a" returned data shifted by
 * the number of bytes skipped in folder 0 (or failed with a truncated
 * CFDATA error), silently extracting wrong file contents.
 *
 * This test fails on unpatched master.
 */
DEFINE_TEST(test_read_format_cab_skip_across_folders)
{
	const char *refname = "test_read_format_cab_skip_across_folders.cab";
	struct archive *a;
	struct archive_entry *ae;
	char buf[40];
	size_t total = 0;
	size_t i;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cab(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 10240));

	/* Entry "a" lives in folder 0; skip it without reading any data. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("a", archive_entry_pathname(ae));
	assertEqualInt(20, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_skip(a));

	/* Entry "b" lives in folder 1 and must return its own bytes. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("b", archive_entry_pathname(ae));
	assertEqualInt(40, archive_entry_size(ae));

	memset(buf, 0, sizeof(buf));
	for (;;) {
		const void *buff;
		size_t size;
		int64_t offset;
		int r = archive_read_data_block(a, &buff, &size, &offset);

		if (r == ARCHIVE_EOF)
			break;
		assertEqualIntA(a, ARCHIVE_OK, r);
		assert(size <= sizeof(buf) - total);
		memcpy(buf + total, buff, size);
		total += size;
	}
	assertEqualInt(40, (int)total);

	/* File "b" must contain the byte sequence 00 01 02 ... 27. */
	for (i = 0; i < total; i++) {
		assertEqualInt((int)i, (int)(unsigned char)buf[i]);
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
