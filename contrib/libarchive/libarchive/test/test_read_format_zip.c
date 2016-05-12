/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2011 Michihiro NAKAJIMA
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
__FBSDID("$FreeBSD$");

/*
 * The reference file for this has been manually tweaked so that:
 *   * file2 has length-at-end but file1 does not
 *   * file2 has an invalid CRC
 */
static void
verify_basic(struct archive *a, int seek_checks)
{
	struct archive_entry *ae;
	char *buff[128];
	const void *pv;
	size_t s;
	int64_t o;

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("dir/", archive_entry_pathname(ae));
	assertEqualInt(1179604249, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_size(ae));
	if (seek_checks)
		assertEqualInt(AE_IFDIR | 0755, archive_entry_mode(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualIntA(a, ARCHIVE_EOF,
	    archive_read_data_block(a, &pv, &s, &o));
	assertEqualInt((int)s, 0);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(1179604289, archive_entry_mtime(ae));
	if (seek_checks)
		assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	assertEqualInt(18, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	failure("archive_read_data() returns number of bytes read");
	if (archive_zlib_version() != NULL) {
		assertEqualInt(18, archive_read_data(a, buff, 19));
		assertEqualMem(buff, "hello\nhello\nhello\n", 18);
	} else {
		assertEqualInt(ARCHIVE_FAILED, archive_read_data(a, buff, 19));
		assertEqualString(archive_error_string(a),
		    "Unsupported ZIP compression method (deflation)");
		assert(archive_errno(a) != 0);
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(1179605932, archive_entry_mtime(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	if (seek_checks) {
		assertEqualInt(AE_IFREG | 0755, archive_entry_mode(ae));
	}
	assert(archive_entry_size_is_set(ae));
	assertEqualInt(18, archive_entry_size(ae));
	if (archive_zlib_version() != NULL) {
		failure("file2 has a bad CRC, so read should fail and not change buff");
		memset(buff, 'a', 19);
		assertEqualInt(ARCHIVE_WARN, archive_read_data(a, buff, 19));
		assertEqualMem(buff, "aaaaaaaaaaaaaaaaaaa", 19);
	} else {
		assertEqualInt(ARCHIVE_FAILED, archive_read_data(a, buff, 19));
		assertEqualString(archive_error_string(a),
		    "Unsupported ZIP compression method (deflation)");
		assert(archive_errno(a) != 0);
	}
	assertEqualInt(ARCHIVE_EOF, archive_read_next_header(a, &ae));
	/* Verify the number of files read. */
	failure("the archive file has three files");
	assertEqualInt(3, archive_file_count(a));
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_ZIP, archive_format(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_basic(void)
{
	const char *refname = "test_read_format_zip.zip";
	struct archive *a;
	char *p;
	size_t s;

	extract_reference_file(refname);

	/* Verify with seeking reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 10240));
	verify_basic(a, 1);

	/* Verify with streaming reader. */
	p = slurpfile(&s, refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory(a, p, s, 31));
	verify_basic(a, 0);
}

/*
 * Read Info-ZIP New Unix Extra Field 0x7875 "ux".
 *  Currently stores Unix UID/GID up to 32 bits.
 */
static void
verify_info_zip_ux(struct archive *a, int seek_checks)
{
	struct archive_entry *ae;
	char *buff[128];

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(1300668680, archive_entry_mtime(ae));
	assertEqualInt(18, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	if (seek_checks)
		assertEqualInt(AE_IFREG | 0644, archive_entry_mode(ae));
	failure("zip reader should read Info-ZIP New Unix Extra Field");
	assertEqualInt(1001, archive_entry_uid(ae));
	assertEqualInt(1001, archive_entry_gid(ae));
	if (archive_zlib_version() != NULL) {
		failure("archive_read_data() returns number of bytes read");
		assertEqualInt(18, archive_read_data(a, buff, 19));
		assertEqualMem(buff, "hello\nhello\nhello\n", 18);
	} else {
		assertEqualInt(ARCHIVE_FAILED, archive_read_data(a, buff, 19));
		assertEqualString(archive_error_string(a),
		    "Unsupported ZIP compression method (deflation)");
		assert(archive_errno(a) != 0);
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify the number of files read. */
	failure("the archive file has just one file");
	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_ZIP, archive_format(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_info_zip_ux(void)
{
	const char *refname = "test_read_format_zip_ux.zip";
	struct archive *a;
	char *p;
	size_t s;

	extract_reference_file(refname);

	/* Verify with seeking reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 10240));
	verify_info_zip_ux(a, 1);

	/* Verify with streaming reader. */
	p = slurpfile(&s, refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory(a, p, s, 108));
	verify_info_zip_ux(a, 0);
}

/*
 * Verify that test_read_extract correctly works with
 * Zip entries that use length-at-end.
 */
static void
verify_extract_length_at_end(struct archive *a, int seek_checks)
{
	struct archive_entry *ae;

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualString("hello.txt", archive_entry_pathname(ae));
	if (seek_checks) {
		assertEqualInt(AE_IFREG | 0644, archive_entry_mode(ae));
		assert(archive_entry_size_is_set(ae));
		assertEqualInt(6, archive_entry_size(ae));
	} else {
		assert(!archive_entry_size_is_set(ae));
		assertEqualInt(0, archive_entry_size(ae));
	}

	if (archive_zlib_version() != NULL) {
		assertEqualIntA(a, ARCHIVE_OK, archive_read_extract(a, ae, 0));
		assertFileContents("hello\x0A", 6, "hello.txt");
	} else {
		assertEqualIntA(a, ARCHIVE_FAILED, archive_read_extract(a, ae, 0));
		assertEqualString(archive_error_string(a),
		    "Unsupported ZIP compression method (deflation)");
		assert(archive_errno(a) != 0);
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_free(a));
}

static void
test_extract_length_at_end(void)
{
	const char *refname = "test_read_format_zip_length_at_end.zip";
	char *p;
	size_t s;
	struct archive *a;

	extract_reference_file(refname);

	/* Verify extraction with seeking reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 10240));
	verify_extract_length_at_end(a, 1);

	/* Verify extraction with streaming reader. */
	p = slurpfile(&s, refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory(a, p, s, 108));
	verify_extract_length_at_end(a, 0);
}

static void
test_symlink(void)
{
	const char *refname = "test_read_format_zip_symlink.zip";
	char *p;
	size_t s;
	struct archive *a;
	struct archive_entry *ae;

	extract_reference_file(refname);
	p = slurpfile(&s, refname);

	/* Symlinks can only be extracted with the seeking reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, p, s, 1));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("symlink", archive_entry_pathname(ae));
	assertEqualInt(AE_IFLNK, archive_entry_filetype(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualString("file", archive_entry_symlink(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_zip)
{
	test_basic();
	test_info_zip_ux();
	test_extract_length_at_end();
	test_symlink();
}
