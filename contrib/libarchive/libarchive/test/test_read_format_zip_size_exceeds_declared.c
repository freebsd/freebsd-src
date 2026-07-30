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
 * Each reference archive has two entries.  The first entry's header
 * declares uncompressed_size=10, but the entry actually contains more
 * data than that:
 *
 *   _stored:  method STORED, compressed_size (which bounds the physical
 *     read for STORED) set to the true 20-byte body.  STORED has no
 *     compressed-stream end marker of its own, so nothing but the
 *     declared size stands between the reader and those extra 10 bytes.
 *
 *   _deflate: method DEFLATE, a real compressed stream that inflates to
 *     256KiB+6 bytes.  This has to exceed the 256KiB internal output
 *     buffer used by zip_read_data_deflate(): a stream that fully
 *     inflates within a single internal decode call also reaches its
 *     own Z_STREAM_END in that same call, so any final-size check runs
 *     before data is handed back to the caller and nothing leaks. Once
 *     decoding needs a second internal call, the first call's full
 *     buffer is handed back as an ordinary chunk before end-of-stream is
 *     ever seen.
 *
 * The second entry ("second", 11 bytes, "hello world") is well-formed.
 * Reading it after the first entry has failed confirms that a
 * mis-declared entry doesn't prevent correctly reading the rest of the
 * archive.
 */

static void
verify(const char *refname)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[8];
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
	assertEqualString("first", archive_entry_pathname(ae));
	assertEqualInt(10, archive_entry_size(ae));
	total = 0;
	while ((n = archive_read_data(a, buff, sizeof(buff))) > 0) {
		total += n;
		failure("archive_read_data() must never return more than "
		    "the declared entry size");
		assert(total <= 10);
	}
	assertEqualInt(ARCHIVE_FAILED, n);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("second", archive_entry_pathname(ae));
	assertEqualInt(11, archive_entry_size(ae));
	total = 0;
	while ((n = archive_read_data(a, buff, sizeof(buff))) > 0)
		total += n;
	assertEqualInt(0, n);
	assertEqualInt(11, total);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Streaming reader: same checks, fed through a non-seekable
	 * memory stream so the source can't be re-read or skipped via
	 * offsets the way a seekable file can. */
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
		assertEqualString("first", archive_entry_pathname(ae));
		assertEqualInt(10, archive_entry_size(ae));
		total = 0;
		while ((n = archive_read_data(a, buff, sizeof(buff))) > 0) {
			total += n;
			failure("archive_read_data() must never return "
			    "more than the declared entry size");
			assert(total <= 10);
		}
		assertEqualInt(ARCHIVE_FAILED, n);

		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_next_header(a, &ae));
		assertEqualString("second", archive_entry_pathname(ae));
		assertEqualInt(11, archive_entry_size(ae));
		total = 0;
		while ((n = archive_read_data(a, buff, sizeof(buff))) > 0)
			total += n;
		assertEqualInt(0, n);
		assertEqualInt(11, total);

		assertEqualIntA(a, ARCHIVE_EOF,
		    archive_read_next_header(a, &ae));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		free(p);
	}
}

DEFINE_TEST(test_read_format_zip_size_exceeds_declared_stored)
{
	verify("test_read_format_zip_size_exceeds_declared_stored.zip");
}

DEFINE_TEST(test_read_format_zip_size_exceeds_declared_deflate)
{
	verify("test_read_format_zip_size_exceeds_declared_deflate.zip");
}
