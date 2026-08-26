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

static void
test_format(int (*set_format)(struct archive *))
{
	struct archive *a;
	struct archive_entry *entry;
	char buff[2048];
	size_t used;

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertA(0 == set_format(a));
	assertA(0 == archive_write_add_filter_none(a));
	assertA(0 == archive_write_set_bytes_per_block(a, 512));
	assertA(0 == archive_write_set_bytes_in_last_block(a, 512));
	assertA(0 == archive_write_open_memory(a, buff, sizeof(buff), &used));

	/* Write directory with empty wide character name */
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_copy_pathname_w(entry, L"");
	archive_entry_set_mode(entry, S_IFDIR | 0755);
	archive_entry_set_uid(entry, 0);
	archive_entry_set_gid(entry, 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));

	/* Close out the archive. */
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/*
	 * Now, read the data back.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &entry));
	assertEqualString("", archive_entry_pathname(entry));

	/* Verify the end of the archive. */
	assertEqualIntA(a, ARCHIVE_EOF,
	    archive_read_next_header(a, &entry));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

}

DEFINE_TEST(test_write_format_tar_empty_dirname)
{
	test_format(archive_write_set_format_gnutar);
	test_format(archive_write_set_format_pax);
	test_format(archive_write_set_format_ustar);
	test_format(archive_write_set_format_v7tar);
}
