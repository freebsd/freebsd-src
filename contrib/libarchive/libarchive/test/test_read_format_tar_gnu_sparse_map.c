/*-SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Martin Mirchev
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
 * Each archive holds a pax entry "file" with GNU.sparse.size=4, a
 * malformed GNU.sparse.map and the four data bytes "ABCD".  The map
 * is rejected with a warning, and the entry is then read as a regular
 * file.
 */
static void
test_malformed_map(const char *refname)
{
	struct archive_entry *ae;
	struct archive *a;
	char buff[16];

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	failure("%s", refname);
	assertEqualIntA(a, ARCHIVE_WARN, archive_read_next_header(a, &ae));
	failure("%s: the warning must have a message", refname);
	assert(archive_error_string(a) != NULL);
	if (archive_error_string(a) != NULL)
		assertEqualString("Malformed sparse map data",
		    archive_error_string(a));
	assertEqualString("file", archive_entry_pathname(ae));
	assertEqualInt(4, archive_entry_size(ae));

	/* The rejected map must not leave a sparse block behind. */
	failure("%s", refname);
	assertEqualInt(4, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem("ABCD", buff, 4);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_tar_gnu_sparse_map)
{
	/* "9223372036854775807,1": offset + size overflows. */
	test_malformed_map("test_read_format_tar_gnu_sparse_map_overflow.tar");
	/* "x": not a number. */
	test_malformed_map("test_read_format_tar_gnu_sparse_map_invalid.tar");
}
