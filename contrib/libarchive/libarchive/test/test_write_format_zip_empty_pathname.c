/*-
 * Copyright (c) 2026 Tim Kientzle
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
 * An empty pathname must be rejected (ARCHIVE_FAILED) by the ZIP writer.
 *
 * write_path() used bitwise & instead of logical && when checking whether
 * to append a trailing slash for directory entries.  Because & does not
 * short-circuit, path[strlen(path)-1] was evaluated even when the entry
 * was not a directory — and when the path is empty, strlen(path)-1 wraps
 * to SIZE_MAX, producing a one-byte OOB read before the heap buffer.
 */
DEFINE_TEST(test_write_format_zip_empty_pathname)
{
	struct archive *a;
	struct archive_entry *ae;
	char buf[4096];
	size_t used;

	/* Regular file with empty pathname */
	assert((a = archive_write_new()) != NULL);
	assertEqualInt(ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualInt(ARCHIVE_OK,
	    archive_write_open_memory(a, buf, sizeof(buf), &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 0);
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Directory entry with empty pathname */
	assert((a = archive_write_new()) != NULL);
	assertEqualInt(ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualInt(ARCHIVE_OK,
	    archive_write_open_memory(a, buf, sizeof(buf), &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_set_size(ae, 0);
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}
