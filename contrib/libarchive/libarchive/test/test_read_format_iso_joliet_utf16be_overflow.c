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
 * A Joliet ISO with deeply nested directories whose UTF-16BE path
 * exactly fills the 1024-byte buffer in build_pathname_utf16be().
 * The next recursive call then writes a 2-byte separator past the
 * end of the allocation (heap-buffer-overflow).
 *
 * Under AddressSanitizer the unfixed code crashes at
 * archive_read_support_format_iso9660.c:3496; with the fix the
 * bounds check before the separator write causes
 * archive_read_next_header() to return ARCHIVE_FATAL.
 */
DEFINE_TEST(test_read_format_iso_joliet_utf16be_overflow)
{
	const char *refname = "test_read_format_iso_joliet_utf16be_overflow.iso";
	struct archive *a;
	struct archive_entry *ae;
	int r;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Read through entries until the overflow path is reached.
	 * With the fix this returns ARCHIVE_FATAL ("Pathname is too long")
	 * instead of writing two bytes past the end of the path buffer. */
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK)
		archive_read_data_skip(a);
	assertEqualInt(ARCHIVE_FATAL, r);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
