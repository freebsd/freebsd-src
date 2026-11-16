/*-
 * Copyright (c) 2026 akshay-bagaria237
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

/*
 * A Rock Ridge "SL" (symlink) record allocates file->symlink, then a
 * later malformed "CE" record makes parse_file_info() take its error
 * path. That path freed file->name but not file->symlink, leaking it.
 */
DEFINE_TEST(test_read_format_iso_rockridge_sl_leak)
{
	const char *refname = "test_read_format_iso_rockridge_sl_leak.iso";
	struct archive *a;
	struct archive_entry *ae;
	void *image;
	size_t size;

	extract_reference_file(refname);
	image = slurpfile(&size, "%s", refname);
	assert(image != NULL);

	a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_iso9660(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, image, size));
	while (archive_read_next_header(a, &ae) == ARCHIVE_OK)
		;
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(image);
}
