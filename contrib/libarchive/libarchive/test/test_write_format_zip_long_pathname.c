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
 * ZIP stores the filename length in a 16-bit field, so the maximum is
 * 65535 bytes.  A pathname of 65536 bytes or more must be rejected
 * (ARCHIVE_FAILED) rather than silently truncated in the header and
 * written past the end of the central-directory segment buffer.
 */
DEFINE_TEST(test_write_format_zip_long_pathname)
{
	struct archive *a;
	struct archive_entry *ae;
	char *pathname;
	char *buf;
	size_t used;
	/* Enough room for a valid ZIP with a 65535-byte filename. */
	const size_t bufsize = 256 * 1024;

	buf = malloc(bufsize);
	assert(buf != NULL);

	/* 65535-byte pathname: exactly the ZIP maximum — must succeed. */
	pathname = malloc(65536);
	assert(pathname != NULL);
	memset(pathname, 'a', 65535);
	pathname[65535] = '\0';

	assert((a = archive_write_new()) != NULL);
	assertEqualInt(ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualInt(ARCHIVE_OK,
	    archive_write_open_memory(a, buf, bufsize, &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, pathname);
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 0);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	free(pathname);

	/* 65536-byte pathname: one byte over the ZIP limit — must be rejected. */
	pathname = malloc(65537);
	assert(pathname != NULL);
	memset(pathname, 'a', 65536);
	pathname[65536] = '\0';

	assert((a = archive_write_new()) != NULL);
	assertEqualInt(ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualInt(ARCHIVE_OK,
	    archive_write_open_memory(a, buf, bufsize, &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, pathname);
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 0);
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	free(pathname);

	/* 131072-byte pathname: the OOB-write size from the bug report. */
	pathname = malloc(131073);
	assert(pathname != NULL);
	memset(pathname, 'a', 131072);
	pathname[131072] = '\0';

	assert((a = archive_write_new()) != NULL);
	assertEqualInt(ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualInt(ARCHIVE_OK,
	    archive_write_open_memory(a, buf, bufsize, &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, pathname);
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 0);
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	free(pathname);

	free(buf);
}
