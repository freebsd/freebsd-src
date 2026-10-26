/*-
 * Copyright (c) 2026 Kaixuan Li
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
 * archive_read_data_block() is public, and a caller that handles sparse
 * entries has to read *offset to place the block; the one in this tree is
 * archive_read_data_into_fd().  A format's read_data handler must
 * therefore write *size and *offset whenever it returns ARCHIVE_OK or
 * ARCHIVE_EOF, including the ARCHIVE_EOF that ends an entry for which it
 * produced no data at all.
 *
 * Both entries below end on an ARCHIVE_EOF that was returned with neither
 * written: rar5_read_data() ends "hardlink.txt" on the first call, at the
 * rar5->file.eof test, and archive_read_format_raw_read_data() returns
 * end-of-file from raw->end_of_file on every call after the first.  The
 * checks seed both outputs and require the handler to replace them.
 */

/*
 * Read to the end of the current entry, checking every block on the way,
 * and return the position the handler reports at ARCHIVE_EOF.
 */
static int64_t
read_entry_to_eof(struct archive *a)
{
	const void *buff;
	size_t size;
	int64_t offset;
	int r;

	for (;;) {
		size = (size_t)-1;
		offset = -1;
		r = archive_read_data_block(a, &buff, &size, &offset);
		failure("read_data must report a size and an offset for "
		    "every block, including the last one");
		assert(size != (size_t)-1);
		assert(offset != -1);
		if (r != ARCHIVE_OK) {
			assertEqualIntA(a, ARCHIVE_EOF, r);
			assertEqualInt(0, size);
			return (offset);
		}
	}
}

DEFINE_TEST(test_read_data_block_offset_at_eof)
{
	const char *rarname = "test_read_format_rar5_hardlink.rar";
	const char *rawname = "test_read_format_raw.data";
	struct archive *a;
	struct archive_entry *ae;
	const void *buff;
	size_t size;
	int64_t offset;

	/*
	 * rar5: "hardlink.txt" carries no data, so the first call to
	 * read_data ends the entry.
	 */
	extract_reference_file(rarname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, rarname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file.txt", archive_entry_pathname(ae));
	assertEqualInt(5, read_entry_to_eof(a));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("hardlink.txt", archive_entry_pathname(ae));
	size = (size_t)-1;
	offset = -1;
	assertEqualIntA(a, ARCHIVE_EOF,
	    archive_read_data_block(a, &buff, &size, &offset));
	failure("rar5 must report a position for an entry it ends on the "
	    "first call");
	assertEqualInt(0, offset);
	assertEqualInt(0, size);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * raw: the entry has data, so it is the second and every later
	 * end-of-file that goes through the short path.
	 */
	extract_reference_file(rawname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, rawname, 512));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("data", archive_entry_pathname(ae));
	assertEqualInt(4, read_entry_to_eof(a));

	size = (size_t)-1;
	offset = -1;
	assertEqualIntA(a, ARCHIVE_EOF,
	    archive_read_data_block(a, &buff, &size, &offset));
	failure("raw must keep reporting a position once the entry has "
	    "ended");
	assertEqualInt(4, offset);
	assertEqualInt(0, size);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
