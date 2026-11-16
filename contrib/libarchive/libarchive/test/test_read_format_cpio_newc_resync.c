/*-
 * Copyright (c) 2026 Corey Daley
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
 * Junk before a newc header is skipped with a warning.  The search for the
 * next header used to step six bytes past a '7', which can be the second
 * or fourth byte of "070701", so for most junk lengths (2-5 mod 6) it ran
 * past the header and every entry after the junk was lost.
 */
DEFINE_TEST(test_read_format_cpio_newc_resync)
{
	static char archive[1024], damaged[1024 + 32];
	struct archive_entry *ae;
	struct archive *a;
	size_t used, second, junk;

	/* Two entries, "a" and "b". */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_cpio_newc(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, archive, sizeof(archive), &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, 0644);
	archive_entry_set_size(ae, 6);
	archive_entry_copy_pathname(ae, "a");
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(6, archive_write_data(a, "first\n", 6));
	archive_entry_copy_pathname(ae, "b");
	archive_entry_set_size(ae, 7);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(7, archive_write_data(a, "second\n", 7));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Where the second header starts. */
	for (second = 6; second + 6 <= used; second++)
		if (memcmp(archive + second, "070701", 6) == 0)
			break;
	assert(second + 6 <= used);

	for (junk = 1; junk <= 12; junk++) {
		memcpy(damaged, archive, second);
		memset(damaged + second, 'J', junk);
		memcpy(damaged + second + junk, archive + second,
		    used - second);

		assert((a = archive_read_new()) != NULL);
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_cpio(a));
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_open_memory(a, damaged, used + junk));
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualString("a", archive_entry_pathname(ae));
		failure("%d bytes of junk", (int)junk);
		assertEqualIntA(a, ARCHIVE_WARN,
		    archive_read_next_header(a, &ae));
		assertEqualString("b", archive_entry_pathname(ae));
		assertEqualIntA(a, ARCHIVE_EOF,
		    archive_read_next_header(a, &ae));
		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	}
}
