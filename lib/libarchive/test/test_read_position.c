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
__FBSDID("$FreeBSD$");

static unsigned char nulls[10000000];
static unsigned char buff[10000000];

/* Check that header_position tracks correctly on read. */
DEFINE_TEST(test_read_position)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t write_pos;
	const size_t data_size = 1000000;

	/* Create a simple archive_entry. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "testfile");
	archive_entry_set_mode(ae, S_IFREG);
	archive_entry_set_size(ae, data_size);

	assert(NULL != (a = archive_write_new()));
	assertA(0 == archive_write_set_format_pax_restricted(a));
	assertA(0 == archive_write_set_bytes_per_block(a, 512));
	assertA(0 == archive_write_open_memory(a, buff, sizeof(buff), &write_pos));
	assertA(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	assertA(data_size == (size_t)archive_write_data(a, nulls, sizeof(nulls)));
#if ARCHIVE_VERSION_NUMBER < 2000000
	assertA(0 == archive_write_close(a));
	archive_write_finish(a);
#else
	assertA(0 == archive_write_finish(a));
#endif
	/* 512-byte header + data_size (rounded up) + 1024 end-of-archive */
	assert(write_pos == ((512 + data_size + 1024 + 511)/512)*512);

	/* Read the archive back. */
	assert(NULL != (a = archive_read_new()));
	assertA(0 == archive_read_support_format_tar(a));
	assertA(0 == archive_read_open_memory2(a, buff, sizeof(buff), 512));
	assert((intmax_t)0 == (intmax_t)archive_read_header_position(a));
	assertA(0 == archive_read_next_header(a, &ae));
	assert((intmax_t)0 == (intmax_t)archive_read_header_position(a));
	assertA(0 == archive_read_data_skip(a));
	assert((intmax_t)0 == (intmax_t)archive_read_header_position(a));
	assertA(1 == archive_read_next_header(a, &ae));
	assert((intmax_t)((data_size + 511 + 512)/512)*512 == (intmax_t)archive_read_header_position(a));
	assertA(0 == archive_read_close(a));
	assert((intmax_t)((data_size + 511 + 512)/512)*512 == (intmax_t)archive_read_header_position(a));
	archive_read_finish(a);
}
