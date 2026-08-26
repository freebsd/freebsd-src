/*-
 * Copyright (c) 2026 tbontb-iaq
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
 * archive_seek_data() returns ARCHIVE_FAILED when the active format
 * does not implement a seek_data hook.  Such "no seeking for this
 * format" is a capability gap, not stream corruption, and the archive
 * must remain usable for subsequent reads.  See #3323.
 */

DEFINE_TEST(test_archive_seek_data_unsupported)
{
	char buff[8192];
	size_t used = 0;
	struct archive *a;
	struct archive_entry *ae;
	char rbuf[64];
	la_ssize_t rd;
	la_int64_t probe;

	/* Build a small ustar archive in memory.  ustar has no seek_data. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "hello.txt");
	archive_entry_set_mode(ae, S_IFREG | 0644);
	archive_entry_set_size(ae, 11);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 11, archive_write_data(a, "hello world", 11));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Read it back. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("hello.txt", archive_entry_pathname(ae));

	/*
	 * Capability probe.  Must report the missing capability with
	 * ARCHIVE_FAILED and set an error string.  3.8.7 and 3.8.8 both
	 * returned ARCHIVE_FATAL here; ARCHIVE_FAILED is the new
	 * contract, because a missing hook is recoverable.
	 */
	probe = archive_seek_data(a, 0, SEEK_CUR);
	failure("archive_seek_data() on a format without a seek_data hook "
	    "must report ARCHIVE_FAILED (return value contract)");
	assertEqualInt(ARCHIVE_FAILED, probe);
	failure("archive_seek_data() must set an error string when "
	    "the format has no seek_data hook");
	assert(archive_error_string(a) != NULL);

	/*
	 * The probe must NOT have poisoned the archive.  Subsequent reads
	 * from the current entry must still succeed and return the real
	 * entry content.  This is the 3.8.8 regression.
	 */
	rd = archive_read_data(a, rbuf, sizeof(rbuf));
	failure("archive_read_data() after a non-fatal capability probe "
	    "must still return the entry data (regression in 3.8.8, "
	    "see GitHub issue #3323)");
	assertEqualInt(11, rd);
	assertEqualMem(rbuf, "hello world", 11);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
