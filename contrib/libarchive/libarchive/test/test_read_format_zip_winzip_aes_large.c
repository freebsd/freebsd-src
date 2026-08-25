/*-
 * Copyright (c) 2013 Konrad Kleine
 * Copyright (c) 2014 Michihiro NAKAJIMA
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

#define FILE_COUNT 4

static const char *FILE_NAMES[FILE_COUNT] = { "Makefile", "NEWS", "README", "config.h" };
static const size_t FILE_SIZE[FILE_COUNT] = { 1456747, 29357, 6818, 32667 };

static void
test_file_with_bad_password(struct archive *a, int file_id, __LA_MODE_T file_permissions)
{
	struct archive_entry *ae;
	char buff[512];

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | file_permissions), archive_entry_mode(ae));
	assertEqualString(FILE_NAMES[file_id], archive_entry_pathname(ae));
	assertEqualInt(FILE_SIZE[file_id], archive_entry_size(ae));
	assertEqualInt(1, archive_entry_is_data_encrypted(ae));
	assertEqualInt(0, archive_entry_is_metadata_encrypted(ae));
	assertEqualIntA(a, 1, archive_read_has_encrypted_entries(a));
	assertEqualInt(ARCHIVE_FAILED, archive_read_data(a, buff, sizeof(buff)));
}

static void
test_file_with_good_password(struct archive *a, int file_id, __LA_MODE_T file_permissions)
{
	struct archive_entry *ae;
	char buff[512];

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | file_permissions), archive_entry_mode(ae));
	assertEqualString(FILE_NAMES[file_id], archive_entry_pathname(ae));
	assertEqualInt(FILE_SIZE[file_id], archive_entry_size(ae));
	assertEqualInt(1, archive_entry_is_data_encrypted(ae));
	assertEqualInt(0, archive_entry_is_metadata_encrypted(ae));
	assertEqualIntA(a, 1, archive_read_has_encrypted_entries(a));
	if (archive_zlib_version() != NULL) {
		assertEqualInt(512, archive_read_data(a, buff, sizeof(buff)));
	} else {
		assertEqualInt(ARCHIVE_FAILED,
					   archive_read_data(a, buff, sizeof(buff)));
		assertEqualString(archive_error_string(a),
						  "Unsupported ZIP compression method (8: deflation)");
		assert(archive_errno(a) != 0);
	}
}

static void
test_archive(const char *refname, const char *password, int good_password, const int file_order[FILE_COUNT], __LA_MODE_T file_permissions)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	if (password != NULL) {
		assertEqualIntA(a, ARCHIVE_OK,
						archive_read_add_passphrase(a, password));
	}
	assertEqualIntA(a, ARCHIVE_OK,
					archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_READ_FORMAT_ENCRYPTION_DONT_KNOW,
					archive_read_has_encrypted_entries(a));

	/* Verify encrypted files */
	for(int i = 0; i < FILE_COUNT; ++i) {
		if (good_password) {
			test_file_with_good_password(a, file_order[i], file_permissions);
		} else {
			test_file_with_bad_password(a, file_order[i], file_permissions);
		}
	}

	assertEqualInt(FILE_COUNT, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_winzip_aes_large_with_file_order(const char *refname, const char *compression_name, __LA_MODE_T file_permissions, const int file_order[FILE_COUNT])
{
	struct archive *a;

	/* Check if running system has cryptographic functionality. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	if (ARCHIVE_OK != archive_write_set_options(a,
				"zip:encryption=aes256")) {
		skipping("This system does not have cryptographic library");
		archive_write_free(a);
		return;
	}

	/* Check if running system supports compression format used */
	if (compression_name && ARCHIVE_OK != archive_write_set_options(a, compression_name)) {
		skipping("%s is not supported on this platform", compression_name);
		archive_write_free(a);
		return;
	}

	archive_write_free(a);

	extract_reference_file(refname);

	/* Extract a zip file without password. */
	test_archive(refname, NULL, 0, file_order, file_permissions);

	/* Extract a zip file with bad password. */
	test_archive(refname, "not_the_password", 0, file_order, file_permissions);

	/* Extract a zip file with good password. */
	test_archive(refname, "password", 1, file_order, file_permissions);
}

static void
test_winzip_aes_large(const char *refname, const char *compression_name)
{
	int file_order[FILE_COUNT] = { 0, 1, 2, 3 };
	test_winzip_aes_large_with_file_order(refname, compression_name, ((__LA_MODE_T)0644), file_order);
}


DEFINE_TEST(test_read_format_zip_winzip_aes256_large)
{
	// Deflate is always supported, no need to test for it
	test_winzip_aes_large("test_read_format_zip_winzip_aes256_large.zip", NULL);
}


DEFINE_TEST(test_read_format_zip_winzip_aes256_large_bzip2)
{
	test_winzip_aes_large("test_read_format_zip_winzip_aes256_large_bzip2.zip", "zip:compression=bzip2");
}

DEFINE_TEST(test_read_format_zip_winzip_aes256_large_lzma)
{
	test_winzip_aes_large("test_read_format_zip_winzip_aes256_large_lzma.zip", "zip:compression=lzma");
}

DEFINE_TEST(test_read_format_zip_winzip_aes256_large_ppmd)
{
	test_winzip_aes_large("test_read_format_zip_winzip_aes256_large_ppmd.zip", NULL);
}

DEFINE_TEST(test_read_format_zip_winzip_aes256_large_xz)
{
	test_winzip_aes_large("test_read_format_zip_winzip_aes256_large_xz.zip", "zip:compression=xz");
}

DEFINE_TEST(test_read_format_zip_winzip_aes256_large_zstd)
{
	int file_order[FILE_COUNT] = { 3, 0, 1, 2 };
	test_winzip_aes_large_with_file_order("test_read_format_zip_winzip_aes256_large_zstd.zip",
										  "zip:compression=zstd", ((__LA_MODE_T)0664), file_order);
}
