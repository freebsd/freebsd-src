/*-
 * Copyright (c) 2003-2024 Tim Kientzle
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

DEFINE_TEST(test_read_format_huge_rpm)
{
	struct archive_entry *ae;
	struct archive *a;
	const char *name = "test_read_format_huge_rpm.rpm";

	assert((a = archive_read_new()) != NULL);
        assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	extract_reference_file(name);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, name, 2));

	/* This archive should have no entries -- if it has entries, the bid has screwed up */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify that the format detection worked. */
	assertEqualInt(ARCHIVE_FILTER_RPM, archive_filter_code(a, 0));
	assertEqualString("rpm", archive_filter_name(a, 0));
	assertEqualInt(ARCHIVE_FORMAT_EMPTY, archive_format(a));

	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

