/*-
 * Copyright (c) 2007 Kai Wang
 * Copyright (c) 2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
__FBSDID("$FreeBSD: head/lib/libarchive/test/test_read_format_raw.c 191594 2009-04-27 20:09:05Z kientzle $");

DEFINE_TEST(test_read_format_raw)
{
	char buff[512];
	struct archive_entry *ae;
	struct archive *a;
	const char *reffile1 = "test_read_format_raw.data";
	const char *reffile2 = "test_read_format_raw.data.Z";
	const char *reffile3 = "test_read_format_raw.bufr";
#ifdef HAVE_ZLIB_H
	const char *reffile4 = "test_read_format_raw.data.gz";
#endif

	/* First, try pulling data out of an uninterpretable file. */
	extract_reference_file(reffile1);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile1, 512));

	/* First (and only!) Entry */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("data", archive_entry_pathname(ae));
	/* Most fields should be unset (unknown) */
	assert(!archive_entry_size_is_set(ae));
	assert(!archive_entry_atime_is_set(ae));
	assert(!archive_entry_ctime_is_set(ae));
	assert(!archive_entry_mtime_is_set(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualInt(4, archive_read_data(a, buff, 32));
	assertEqualMem(buff, "foo\n", 4);

	/* Test EOF */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));


	/* Second, try the same with a compressed file. */
	extract_reference_file(reffile2);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_by_code(a, ARCHIVE_FORMAT_RAW));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile2, 1));

	/* First (and only!) Entry */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("data", archive_entry_pathname(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	/* Most fields should be unset (unknown) */
	assert(!archive_entry_size_is_set(ae));
	assert(!archive_entry_atime_is_set(ae));
	assert(!archive_entry_ctime_is_set(ae));
	assert(!archive_entry_mtime_is_set(ae));
	assertEqualInt(4, archive_read_data(a, buff, 32));
	assertEqualMem(buff, "foo\n", 4);

	/* Test EOF */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));


	/* Third, try with file which fooled us in past - appeared to be tar. */
	extract_reference_file(reffile3);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_set_format(a, ARCHIVE_FORMAT_RAW));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile3, 1));

	/* First (and only!) Entry */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("data", archive_entry_pathname(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	/* Most fields should be unset (unknown) */
	assert(!archive_entry_size_is_set(ae));
	assert(!archive_entry_atime_is_set(ae));
	assert(!archive_entry_ctime_is_set(ae));
	assert(!archive_entry_mtime_is_set(ae));

	/* Test EOF */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

#ifdef HAVE_ZLIB_H
	/* Fourth, try with gzip which has metadata. */
	extract_reference_file(reffile4);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, reffile4, 1));

	/* First (and only!) Entry */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("test-file-name.data", archive_entry_pathname(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assert(archive_entry_mtime_is_set(ae));
	assertEqualIntA(a, archive_entry_mtime(ae), 0x5cbafd25);
	/* Most fields should be unset (unknown) */
	assert(!archive_entry_size_is_set(ae));
	assert(!archive_entry_atime_is_set(ae));
	assert(!archive_entry_ctime_is_set(ae));

	/* Test EOF */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
#endif
}
