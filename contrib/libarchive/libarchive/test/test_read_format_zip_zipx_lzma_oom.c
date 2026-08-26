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
 * A ZIPX entry whose LZMA properties encode a 0xFFFFFFFF-byte dictionary
 * previously caused libarchive to attempt a ~4 GiB allocation because
 * lzma_alone_decoder() was called with an unlimited memory limit.
 * The fix passes a 64 MiB limit; the decoder must then return
 * LZMA_MEMLIMIT_ERROR, which libarchive must report as ENOMEM.
 */
DEFINE_TEST(test_read_format_zip_zipx_lzma_oom)
{
	const char *refname = "test_read_format_zip_zipx_lzma_oom.zipx";
	struct archive *a;
	struct archive_entry *ae;
	char buf[64];

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping("lzma reading not fully supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	/*
	 * Data extraction must fail gracefully with ENOMEM rather than
	 * attempting a multi-gigabyte allocation.
	 */
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_data(a, buf, sizeof(buf)));
	assertEqualInt(ENOMEM, archive_errno(a));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
