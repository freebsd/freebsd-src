/*-
 * Copyright (c) 2025
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
 * Verify that a crafted ISO9660 image with an invalid zisofs block-size
 * exponent (pz_log2_bs) is handled gracefully.
 *
 * The ZF extension in the Rock Ridge entry stores pz_log2_bs as a raw
 * byte from the image.  The zisofs spec only permits values 15-17.
 * Values outside that range can cause:
 *   - Undefined behavior via oversized bit shifts (any platform)
 *   - Integer overflow in block pointer allocation on 32-bit platforms,
 *     leading to a heap buffer overflow write
 *
 * The test image has pz_log2_bs=2 (out of spec) combined with
 * pz_uncompressed_size=0xFFFFFFF9.  On 32-bit, (ceil+1)*4 overflows
 * size_t to 0, malloc(0) returns a tiny buffer, and the code attempts
 * to write ~4GB into it.  On 64-bit the allocation is huge and safely
 * fails.
 *
 * We verify the fix by checking archive_entry_size() after reading the
 * header.  When pz_log2_bs validation rejects the bad value (pz=0),
 * the entry keeps its raw on-disk size (small).  Without the fix,
 * the reader sets the entry size to pz_uncompressed_size (0xFFFFFFF9).
 *
 * We intentionally do NOT call archive_read_data() here.  Without the
 * fix, the data-read path triggers a heap buffer overflow on 32-bit
 * that silently corrupts the process heap, causing later tests to
 * crash rather than this one.
 */
DEFINE_TEST(test_read_format_iso_zisofs_overflow)
{
	const char reffile[] = "test_read_format_iso_zisofs_overflow.iso";
	struct archive *a;
	struct archive_entry *ae;
	int r = ARCHIVE_OK;
	int found_regular_file = 0;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile, 10240));

	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK ||
	    r == ARCHIVE_WARN) {
		/*
		 * With the fix, pz_log2_bs=2 is rejected and pz is set
		 * to 0, so the entry keeps its small raw size from the
		 * ISO directory record.  Without the fix, zisofs sets
		 * the entry size to pz_uncompressed_size (0xFFFFFFF9).
		 *
		 * We intentionally do NOT call archive_read_data().
		 * Without the fix, the data-read path triggers a heap
		 * buffer overflow on 32-bit that silently corrupts the
		 * process heap, causing later tests to crash rather
		 * than this one.
		 */
		if (archive_entry_filetype(ae) == AE_IFREG) {
			la_int64_t sz = archive_entry_size(ae);
			failure("entry \"%s\" has size %jd"
			    "; expected < 1 MiB"
			    " (if size is 4294966265 = 0xFFFFFFF9, the"
			    " pz_log2_bs validation is missing)",
			    archive_entry_pathname(ae), (intmax_t)sz);
			assert(sz < 1024 * 1024);
			found_regular_file = 1;
		}
	}

	/* Iteration must have completed normally. */
	assertEqualInt(ARCHIVE_EOF, r);

	/* The PoC image contains a regular file; if we never saw one,
	 * something is wrong with the test image. */
	assert(found_regular_file);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
