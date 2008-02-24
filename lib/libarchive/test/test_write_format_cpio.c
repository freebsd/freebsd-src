/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
__FBSDID("$FreeBSD: src/lib/libarchive/test/test_write_format_cpio.c,v 1.2 2007/07/06 15:43:11 kientzle Exp $");

/* The version stamp macro was introduced after cpio write support. */
#if ARCHIVE_VERSION_STAMP >= 1009000
static void
test_format(int	(*set_format)(struct archive *))
{
	char filedata[64];
	struct archive_entry *ae;
	struct archive *a;
	char *p;
	size_t used;
	size_t buffsize = 1000000;
	char *buff;

	buff = malloc(buffsize);

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertA(0 == (*set_format)(a));
	assertA(0 == archive_write_set_compression_none(a));
	assertA(0 == archive_write_open_memory(a, buff, buffsize, &used));

	/*
	 * Write a file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 10);
	assert(1 == archive_entry_mtime(ae));
	assert(10 == archive_entry_mtime_nsec(ae));
	p = strdup("file");
	archive_entry_copy_pathname(ae, p);
	strcpy(p, "XXXX");
	free(p);
	assertEqualString("file", archive_entry_pathname(ae));
	archive_entry_set_mode(ae, S_IFREG | 0755);
	assert((S_IFREG | 0755) == archive_entry_mode(ae));
	archive_entry_set_size(ae, 8);

	assertA(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	assertA(8 == archive_write_data(a, "12345678", 9));

	/* Close out the archive. */
	assertA(0 == archive_write_close(a));
#if ARCHIVE_API_VERSION > 1
	assertA(0 == archive_write_finish(a));
#else
	archive_write_finish(a);
#endif

	/*
	 * Now, read the data back.
	 */
	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_support_compression_all(a));
	assertA(0 == archive_read_open_memory(a, buff, used));

	assertA(0 == archive_read_next_header(a, &ae));

	assert(1 == archive_entry_mtime(ae));
	/* Not the same as above: cpio doesn't store hi-res times. */
	assert(0 == archive_entry_mtime_nsec(ae));
	assert(0 == archive_entry_atime(ae));
	assert(0 == archive_entry_ctime(ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assert((S_IFREG | 0755) == archive_entry_mode(ae));
	assert(8 == archive_entry_size(ae));
	assertA(8 == archive_read_data(a, filedata, 10));
	assert(0 == memcmp(filedata, "12345678", 8));

	/* Verify the end of the archive. */
	assert(1 == archive_read_next_header(a, &ae));
	assert(0 == archive_read_close(a));
#if ARCHIVE_API_VERSION > 1
	assert(0 == archive_read_finish(a));
#else
	archive_read_finish(a);
#endif

	free(buff);
}
#endif

DEFINE_TEST(test_write_format_cpio)
{
#if ARCHIVE_VERSION_STAMP >= 1009000
	test_format(archive_write_set_format_cpio);
	test_format(archive_write_set_format_cpio_newc);
#else
	skipping("cpio write support");
#endif
}
