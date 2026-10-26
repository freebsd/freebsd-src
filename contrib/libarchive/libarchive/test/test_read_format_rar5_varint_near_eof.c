/*-
 * Copyright (c) 2026 Gaurav Jadhav
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
 * Regression test for GitHub issue #3484.
 *
 * read_var() always calls read_ahead(a, 10, &p), requiring a full 10-byte
 * lookahead before decoding a varint, even though RAR5 varints are
 * self-terminating (a byte with the MSB clear ends the value, usually
 * within 1-3 bytes). In this single-entry archive the FILE header's extra
 * area size is encoded as a 1-byte varint that sits fewer than 10 bytes
 * from the end of the archive (ENDARC immediately follows, with no
 * padding), which is a legal layout. read_ahead(a, 10, &p) can't satisfy
 * the full 10-byte request, so read_var() returned 0 even though the
 * varint's bytes were fully present and valid, and every caller treated
 * that failure identically to legitimate end-of-archive: the entry was
 * silently dropped and archive_read_next_header() returned ARCHIVE_EOF.
 */
DEFINE_TEST(test_read_format_rar5_varint_near_eof)
{
	/* Minimal single-entry RAR5 archive: signature, main archive
	 * header, a FILE header for entry "a" (empty, stored) whose extra
	 * area size varint sits right up against the trailing ENDARC
	 * block, then ENDARC itself. */
	static const uint8_t data[] = {
		0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x01, 0x00,
		0xc5, 0x1a, 0x33, 0x32, 0x03, 0x01, 0x00, 0x00,
		0x1a, 0xc7, 0x20, 0x59, 0x0d, 0x02, 0x03, 0x02,
		0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x61,
		0x01, 0x64, 0x00,
		0x39, 0xf9, 0xb2, 0x81, 0x02, 0x05, 0x00
	};

	struct archive *a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_rar5(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a,
	    data, sizeof(data)));

	struct archive_entry *ae;
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("a", archive_entry_pathname(ae));

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
