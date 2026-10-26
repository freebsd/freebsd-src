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
 * Regression test for ignored parse_htime_item() failures in
 * parse_file_extra_htime().
 *
 * The HTIME extra record of the FILE header below declares a unix mtime,
 * ctime and atime, but the archive ends right after the mtime and ctime
 * values, so reading the atime fails with ARCHIVE_EOF.  The unpatched
 * code ignored that failure: the extra area accounting then went
 * negative and the reader reported a misleading ARCHIVE_FATAL
 * "unsupported structure of file header extra data".  The patched code
 * propagates the EOF instead.
 */
DEFINE_TEST(test_read_format_rar5_truncated_htime)
{
	/* RAR5 archive with a FILE header ("a") whose HTIME extra record is
	 * truncated inside the atime field. */
	static const uint8_t data[] = {
		0x52,0x61,0x72,0x21,0x1a,0x07,0x01,0x00,
		0x01,0xa1,0xc1,0x64,0x17,0x02,0x03,0x03,
		0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x61,
		0x00,0x03,0x0f,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00
	};

	struct archive *a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_rar5(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a,
	    data, sizeof(data)));

	struct archive_entry *ae;
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
