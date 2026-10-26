/*-
 * Copyright (c) 2026 susen325
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
 * The local header of an lzma (zipx) entry claims a compressed size of
 * about 4 GiB while the archive is under 2 KiB.  Skipping the trailing
 * compressed bytes must not request a read-ahead buffer of that size.
 */
DEFINE_TEST(test_read_format_zip_lzma_alone_large_size)
{
	const char *refname = "test_read_format_zip_lzma_alone_large_size.zip";
	struct archive *a;
	struct archive_entry *ae;
	char buf[1024];
	la_ssize_t s;
	int r;

	extract_reference_file(refname);
	a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK ||
	    r == ARCHIVE_WARN) {
		while ((s = archive_read_data(a, buf, sizeof(buf))) > 0)
			;
		assert(s != ARCHIVE_FATAL);
	}
	assertEqualIntA(a, ARCHIVE_EOF, r);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
