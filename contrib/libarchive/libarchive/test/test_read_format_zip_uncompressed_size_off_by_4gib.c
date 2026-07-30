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
 * The reference archive has one STORED entry, "hello.txt", holding the 5
 * real bytes "hello". Both 32-bit size fields are the zip64 sentinel
 * (0xffffffff); the zip64 extra field is spec-valid and resolves
 * compressed_size to the true 5, but resolves uncompressed_size to
 * 2^32+5 (4294967301) -- a lie that differs from the truth by an exact
 * multiple of 2^32.
 *
 * archive_read_format_zip_read_data()'s end-of-entry check compared
 * declared vs. actual uncompressed size masked to the low 32 bits, so
 * this lie went undetected: the entry reports a ~4GiB size via
 * archive_entry_size() (unchanged by this fix -- that's a header-time
 * concern, not this one), but silently completes extraction having
 * delivered only 5 bytes. The fix compares the full 64-bit values, so
 * extraction now fails once the size mismatch is discovered at
 * end-of-entry.
 */
static void
verify(const char *refname)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[16];
	la_ssize_t n;
	int64_t total;

	/* Seeking reader. */
	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("hello.txt", archive_entry_pathname(ae));
	assertEqualInt(4294967301LL, archive_entry_size(ae));
	total = 0;
	while ((n = archive_read_data(a, buff, sizeof(buff))) > 0)
		total += n;
	failure("must never report success after silently delivering fewer "
	    "bytes than the declared size");
	assertEqualInt(ARCHIVE_FAILED, n);
	assert(total <= 4294967301LL);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Streaming reader. */
	{
		size_t s;
		char *p = slurpfile(&s, "%s", refname);
		assert((a = archive_read_new()) != NULL);
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_filter_all(a));
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_all(a));
		assertEqualIntA(a, ARCHIVE_OK, read_open_memory(a, p, s, 7));

		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_next_header(a, &ae));
		assertEqualString("hello.txt", archive_entry_pathname(ae));
		assertEqualInt(4294967301LL, archive_entry_size(ae));
		total = 0;
		while ((n = archive_read_data(a, buff, sizeof(buff))) > 0)
			total += n;
		assertEqualInt(ARCHIVE_FAILED, n);
		assert(total <= 4294967301LL);

		assertEqualIntA(a, ARCHIVE_EOF,
		    archive_read_next_header(a, &ae));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		free(p);
	}
}

DEFINE_TEST(test_read_format_zip_uncompressed_size_off_by_4gib)
{
	verify("test_read_format_zip_uncompressed_size_off_by_4gib.zip");
}
