/*
 * Copyright (c) 2003-2018
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

/*
 * All of the archives for this test contain four files: a.txt, b.txt, c.txt, and d.txt
 * For solid archives or archives or archives where filenames are encrypted, all four files are encrypted with the
 * password "password". For non-solid archives without filename encryption, a.txt and c.txt are not encrypted, b.txt is
 * encrypted with the password "password", and d.txt is encrypted with the password "password2".
 *
 * For all files the file contents is "This is from <filename>" (i.e. "This is from a.txt" etc.)
 */
static void test_encrypted_rar_archive(const char *filename, int filenamesEncrypted, int solid)
{
	struct archive_entry *ae;
	struct archive *a;
	char buff[128];
	int expected_read_header_result, expected_read_data_result;
	const int expected_file_size = 18; /* This is from X.txt */

	/* We should only ever fail to read the header when filenames are encrypted. Otherwise we're failing for other
	 * unsupported reasons, in which case we have no hope of detecting encryption */
	expected_read_header_result = filenamesEncrypted ? ARCHIVE_FATAL : ARCHIVE_OK;

	/* We should only ever fail to read the data for "a.txt" and "c.txt" if they are encrypted */
	/* NOTE: We'll never attempt this when filenames are encrypted, so we only check for solid here */
	expected_read_data_result = solid ? ARCHIVE_FATAL : expected_file_size;

	extract_reference_file(filename);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, filename, 10240));

	/* No data read yet; encryption unknown */
	assertEqualInt(ARCHIVE_READ_FORMAT_ENCRYPTION_DONT_KNOW, archive_read_has_encrypted_entries(a));

	/* Read the header for "a.txt" */
	assertEqualIntA(a, expected_read_header_result, archive_read_next_header(a, &ae));
	if (!filenamesEncrypted) {
		assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
		assertEqualString("a.txt", archive_entry_pathname(ae));
		assertEqualInt(expected_file_size, archive_entry_size(ae));
		assertEqualInt(solid, archive_entry_is_data_encrypted(ae));
		assertEqualInt(0, archive_entry_is_metadata_encrypted(ae));
		/* NOTE: The reader will set this value to zero on the first header that it reads, even if later entries
		 * are encrypted */
		assertEqualInt(solid, archive_read_has_encrypted_entries(a));
		assertEqualIntA(a, expected_read_data_result, archive_read_data(a, buff, sizeof(buff)));
		if (!solid) {
			assertEqualMem("This is from a.txt", buff, expected_file_size);
		}
	}
	else {
		assertEqualInt(1, archive_entry_is_data_encrypted(ae));
		assertEqualInt(1, archive_entry_is_metadata_encrypted(ae));
		assertEqualInt(1, archive_read_has_encrypted_entries(a));
		assertEqualInt(ARCHIVE_FATAL, archive_read_data(a, buff, sizeof(buff)));

		/* Additional attempts to read headers are futile */
		assertEqualInt(ARCHIVE_OK, archive_read_close(a));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return;
	}

	/* From here on out, we can assume that !filenamesEncrypted */

	/* Read the header for "b.txt" */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("b.txt", archive_entry_pathname(ae));
	assertEqualInt(expected_file_size, archive_entry_size(ae));
	assertEqualInt(1, archive_entry_is_data_encrypted(ae));
	assertEqualInt(0, archive_entry_is_metadata_encrypted(ae));
	assertEqualInt(1, archive_read_has_encrypted_entries(a));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_data(a, buff, sizeof(buff)));

	/* Read the header for "c.txt" */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("c.txt", archive_entry_pathname(ae));
	assertEqualInt(expected_file_size, archive_entry_size(ae));
	assertEqualInt(solid, archive_entry_is_data_encrypted(ae));
	assertEqualInt(0, archive_entry_is_metadata_encrypted(ae));
	/* After setting to true above, this should forever be true */
	assertEqualInt(1, archive_read_has_encrypted_entries(a));
	assertEqualIntA(a, expected_read_data_result, archive_read_data(a, buff, sizeof(buff)));
	if (!solid) {
		assertEqualMem("This is from c.txt", buff, expected_file_size);
	}

	/* Read the header for "d.txt" */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("d.txt", archive_entry_pathname(ae));
	assertEqualInt(expected_file_size, archive_entry_size(ae));
	assertEqualInt(1, archive_entry_is_data_encrypted(ae));
	assertEqualInt(0, archive_entry_is_metadata_encrypted(ae));
	assertEqualInt(1, archive_read_has_encrypted_entries(a));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_data(a, buff, sizeof(buff)));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Close the archive. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_rar4_encrypted)
{
	test_encrypted_rar_archive("test_read_format_rar4_encrypted.rar", 0, 0);
}

DEFINE_TEST(test_read_format_rar4_encrypted_filenames)
{
	test_encrypted_rar_archive("test_read_format_rar4_encrypted_filenames.rar", 1, 0);
}

DEFINE_TEST(test_read_format_rar4_solid_encrypted)
{
	/* TODO: If solid RAR4 support is ever added, the following should pass */
#if 0
	test_encrypted_rar_archive("test_read_format_rar4_solid_encrypted.rar", 0, 1);
#else
	skipping("RAR4 solid archive support not currently available");
#endif
}

DEFINE_TEST(test_read_format_rar4_solid_encrypted_filenames)
{
	test_encrypted_rar_archive("test_read_format_rar4_solid_encrypted_filenames.rar", 1, 1);
}

DEFINE_TEST(test_read_format_rar5_encrypted)
{
	test_encrypted_rar_archive("test_read_format_rar5_encrypted.rar", 0, 0);
}

DEFINE_TEST(test_read_format_rar5_encrypted_filenames)
{
	test_encrypted_rar_archive("test_read_format_rar5_encrypted_filenames.rar", 1, 0);
}

DEFINE_TEST(test_read_format_rar5_solid_encrypted)
{
	test_encrypted_rar_archive("test_read_format_rar5_solid_encrypted.rar", 0, 1);
}

DEFINE_TEST(test_read_format_rar5_solid_encrypted_filenames)
{
	test_encrypted_rar_archive("test_read_format_rar5_solid_encrypted_filenames.rar", 1, 1);
}
