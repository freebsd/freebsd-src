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

DEFINE_TEST(test_read_format_cab_lzx_16bit)
{
	const char *refname = "test_read_format_cab_lzx_16bit.cab";
	struct archive *a;
	struct archive_entry *ae;
	char buff[16];

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cab(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Read entry. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(archive_entry_pathname(ae), "lzx16.bin");
	assertEqualInt(archive_entry_size(ae), 16);
	assertEqualIntA(a, sizeof(buff),
	    archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "ABABABABABABABAB", 16);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
