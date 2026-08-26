/*-
 * Copyright (c) 2026 Kaif Khan
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
 * parse_rockridge_ZF1() reads data[0] and data[1] before checking that the
 * "ZF" entry is long enough (the data_length == 12 test was the last term of
 * the condition).  A "ZF" entry with a length byte of 5 yields data_length 1,
 * so data[1] reads one byte past the SUSP area validated by parse_rockridge().
 *
 * The reference image carries the "ZF" entry in a "CE" continuation block that
 * is the final logical block of the image, so the over-read lands one byte
 * past the end of the buffer that holds the block.  Reading the image from an
 * exactly-sized memory buffer makes the over-read observable to AddressSanitizer
 * (a padded buffer would hide it).  With the fix the short entry is rejected and
 * the read completes.
 */
DEFINE_TEST(test_read_format_iso_rockridge_zf_overflow)
{
	const char *refname = "test_read_format_iso_rockridge_zf_overflow.iso";
	struct archive *a;
	struct archive_entry *ae;
	char *buff, *exact;
	size_t used;
	int r;

	extract_reference_file(refname);

	/* slurpfile() over-allocates by one byte; copy into an exactly-sized
	 * buffer so a single-byte over-read is not masked. */
	buff = slurpfile(&used, "%s", refname);
	assert(buff != NULL);
	assert(used > 0);
	assert((exact = malloc(used)) != NULL);
	memcpy(exact, buff, used);
	free(buff);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, exact, used));

	/* Walk the image to completion.  The malformed "ZF" entry is parsed
	 * while reading the root directory's continuation block; before the
	 * fix this over-reads the block buffer. */
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK ||
	    r == ARCHIVE_WARN)
		archive_read_data_skip(a);
	assert(r == ARCHIVE_EOF || r == ARCHIVE_FATAL);

	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(exact);
}
