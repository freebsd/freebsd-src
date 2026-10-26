/*-
 * Copyright (c) 2026 krishna28238-arch
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
 * Regression test for a size_t wraparound in process_base_block().
 *
 * The size vint of a base block can decode to values close to SIZE_MAX,
 * because read_var_sized() does not bound the decoded value.  Computing
 * hdr_size = raw_hdr_size + hdr_size_len then wraps around to a small
 * number, which passes the 2 MB sanity check.  The reader accepted the
 * block and desynced; on the 22-byte archive below, the unpatched reader
 * consumed the size vint as a whole block and reported ARCHIVE_EOF.
 *
 * The patched reader bounds raw_hdr_size before the addition and rejects
 * the block up front.
 */
DEFINE_TEST(test_read_format_rar5_header_size_wraparound)
{
	/* RAR5 signature, CRC32 of the first nine 0xFF bytes, and a size
	 * vint that decodes to SIZE_MAX (0xFF * 9, 0x01). */
	static const uint8_t data[] = {
		0x52,0x61,0x72,0x21,0x1a,0x07,0x01,0x00,
		0x90,0x18,0x20,0xeb,0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,0xff,0x01
	};

	struct archive *a;
	struct archive_entry *ae;

	a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_rar5(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a,
	    data, sizeof(data)));

	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assert(archive_error_string(a) != NULL);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
