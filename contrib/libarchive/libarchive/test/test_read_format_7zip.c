/*-
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

#if HAVE_LZMA_H
#include <lzma.h>
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#define	close		_close
#define	open		_open
#endif

#define __LIBARCHIVE_BUILD

/*
 * Extract a non-encoded file.
 * The header of the 7z archive files is not encoded.
 */
static void
test_copy(int use_open_fd)
{
	const char *refname = "test_read_format_7zip_copy.7z";
	struct archive_entry *ae;
	struct archive *a;
	char buff[128];
	int fd = -1;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	if (use_open_fd) {
		fd = open(refname, O_RDONLY | O_BINARY); 
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_open_fd(a, fd, 10240));
	} else {
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_open_filename(a, refname, 10240));
	}

	/* Verify regular file1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(60, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assert(archive_read_has_encrypted_entries(a) > ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualInt(60, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "    ", 4);

	assertEqualInt(1, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	if (fd != -1)
		close(fd);
}

/*
 * An archive file has no entry.
 */
static void
test_empty_archive(void)
{
	const char *refname = "test_read_format_7zip_empty_archive.7z";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualInt(0, archive_file_count(a));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * An archive file has one empty file. It means there is no content
 * in the archive file except for a header.
 */
static void
test_empty_file(void)
{
	const char *refname = "test_read_format_7zip_empty_file.7z";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify regular empty. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("empty", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	assertEqualInt(1, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_archive_properties)
{
	const char *refname = "test_read_format_7zip_archive_properties.7z";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("empty", archive_entry_pathname(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Extract an encoded file.
 * The header of the 7z archive files is not encoded.
 */
static void
test_plain_header(const char *refname)
{
	struct archive_entry *ae;
	struct archive *a;
	char buff[128];

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify regular file1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(1322058763, archive_entry_mtime(ae));
	assertEqualInt(2844, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(sizeof(buff), archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "The libarchive distribution ", 28);

	assertEqualInt(1, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Extract multi files.
 * The header of the 7z archive files is encoded with LZMA.
 */
static void
test_extract_all_files(const char *refname)
{
	struct archive_entry *ae;
	struct archive *a;
	char buff[128];

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify regular file1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("dir1/file1", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(13, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(13, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\n", 13);

	/* Verify regular file2. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(26, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(26, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\nbbbbbbbbbbbb\n", 26);

	/* Verify regular file3. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file3", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(39, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(39, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\nbbbbbbbbbbbb\ncccccccccccc\n", 39);

	/* Verify regular file4. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file4", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(52, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(52, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff,
	    "aaaaaaaaaaaa\nbbbbbbbbbbbb\ncccccccccccc\ndddddddddddd\n", 52);

	/* Verify directory dir1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFDIR | 0755), archive_entry_mode(ae));
	assertEqualString("dir1/", archive_entry_pathname(ae));
	assertEqualInt(2764801, archive_entry_mtime(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	assertEqualInt(5, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Extract multi files.
 * Like test_extract_all_files, but with zstandard compression.
 */
static void
test_extract_all_files_zstd(const char *refname)
{
	struct archive_entry *ae;
	struct archive *a;
	char buff[128];

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify directory dir1. Note that this comes before the dir1/file1 entry in recent versions of 7-Zip. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFDIR | 0755), archive_entry_mode(ae));
	assertEqualString("dir1/", archive_entry_pathname(ae));
	assertEqualInt(2764801, archive_entry_mtime(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	/* Verify regular file1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("dir1/file1", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(13, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(13, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\n", 13);

	/* Verify regular file2. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(26, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(26, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\nbbbbbbbbbbbb\n", 26);

	/* Verify regular file3. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file3", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(39, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(39, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\nbbbbbbbbbbbb\ncccccccccccc\n", 39);

	/* Verify regular file4. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file4", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(52, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(52, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff,
	    "aaaaaaaaaaaa\nbbbbbbbbbbbb\ncccccccccccc\ndddddddddddd\n", 52);

	assertEqualInt(5, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Extract file from an archives using ZSTD compression with and without BCJ.
 */
static void
test_extract_file_zstd_bcj_nobjc(const char *refname)
{
	struct archive_entry *ae;
	struct archive *a;
	char buff[4096];
	uint32_t computed_crc = 0;
	uint32_t expected_crc = 0xbd66eebc;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify regular file: hw. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0775), archive_entry_mode(ae));
	assertEqualString("hw", archive_entry_pathname(ae));
	assertEqualInt(1685913368, archive_entry_mtime(ae));
	assertEqualInt(15952, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	for (;;) {
		la_ssize_t bytes_read = archive_read_data(a, buff, sizeof(buff));
		assert(bytes_read >= 0);
		if (bytes_read == 0) break;
		computed_crc = bitcrc32(computed_crc, buff, bytes_read);
	}
	assertEqualInt(computed_crc, expected_crc);

	assertEqualInt(1, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Extract last file.
 * The header of the 7z archive files is encoded with LZMA.
 */
static void
test_extract_last_file(const char *refname)
{
	struct archive_entry *ae;
	struct archive *a;
	char buff[128];

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify regular file1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("dir1/file1", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(13, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	/* Verify regular file2. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(26, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	/* Verify regular file3. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file3", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(39, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	/* Verify regular file4. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file4", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(52, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(52, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff,
	    "aaaaaaaaaaaa\nbbbbbbbbbbbb\ncccccccccccc\ndddddddddddd\n", 52);

	/* Verify directory dir1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFDIR | 0755), archive_entry_mode(ae));
	assertEqualString("dir1/", archive_entry_pathname(ae));
	assertEqualInt(2764801, archive_entry_mtime(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	assertEqualInt(5, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Extract a mixed archive file which has both LZMA and LZMA2 encoded files.
 *  LZMA: file1, file2, file3, file4
 *  LZMA2: zfile1, zfile2, zfile3, zfile4
 */
static void
test_extract_all_files2(const char *refname)
{
	struct archive_entry *ae;
	struct archive *a;
	char buff[128];

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify regular file1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("dir1/file1", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(13, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(13, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\n", 13);

	/* Verify regular file2. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(26, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(26, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\nbbbbbbbbbbbb\n", 26);

	/* Verify regular file3. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file3", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(39, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(39, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\nbbbbbbbbbbbb\ncccccccccccc\n", 39);

	/* Verify regular file4. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file4", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(52, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(52, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff,
	    "aaaaaaaaaaaa\nbbbbbbbbbbbb\ncccccccccccc\ndddddddddddd\n", 52);

	/* Verify regular zfile1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("dir1/zfile1", archive_entry_pathname(ae));
	assertEqualInt(5184001, archive_entry_mtime(ae));
	assertEqualInt(13, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(13, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\n", 13);

	/* Verify regular zfile2. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("zfile2", archive_entry_pathname(ae));
	assertEqualInt(5184001, archive_entry_mtime(ae));
	assertEqualInt(26, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(26, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\nbbbbbbbbbbbb\n", 26);

	/* Verify regular zfile3. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("zfile3", archive_entry_pathname(ae));
	assertEqualInt(5184001, archive_entry_mtime(ae));
	assertEqualInt(39, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(39, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "aaaaaaaaaaaa\nbbbbbbbbbbbb\ncccccccccccc\n", 39);

	/* Verify regular zfile4. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("zfile4", archive_entry_pathname(ae));
	assertEqualInt(5184001, archive_entry_mtime(ae));
	assertEqualInt(52, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(52, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff,
	    "aaaaaaaaaaaa\nbbbbbbbbbbbb\ncccccccccccc\ndddddddddddd\n", 52);

	/* Verify directory dir1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFDIR | 0755), archive_entry_mode(ae));
	assertEqualString("dir1/", archive_entry_pathname(ae));
	assertEqualInt(2764801, archive_entry_mtime(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	assertEqualInt(9, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Extract a file compressed with DELTA + LZMA[12].
 */
static void
test_delta_lzma(const char *refname)
{
	struct archive_entry *ae;
	struct archive *a;
	size_t remaining;
	ssize_t bytes;
	char buff[1024];

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify regular file1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(172802, archive_entry_mtime(ae));
	assertEqualInt(27627, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	remaining = (size_t)archive_entry_size(ae);
	while (remaining) {
		if (remaining < sizeof(buff))
			assertEqualInt(remaining,
			    bytes = archive_read_data(a, buff, sizeof(buff)));
		else
			assertEqualInt(sizeof(buff),
			    bytes = archive_read_data(a, buff, sizeof(buff)));
		if (bytes > 0)
			remaining -= bytes;
		else
			break;
	}
	assertEqualInt(0, remaining);

	assertEqualInt(1, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Extract a file compressed with BCJ + LZMA2.
 */
static void
test_bcj(const char *refname)
{
	struct archive_entry *ae;
	struct archive *a;
	size_t remaining;
	ssize_t bytes;
	char buff[1024];

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify regular x86exe. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0444), archive_entry_mode(ae) & ~0111);
	assertEqualString("x86exe", archive_entry_pathname(ae));
	assertEqualInt(172802, archive_entry_mtime(ae));
	assertEqualInt(27328, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	remaining = (size_t)archive_entry_size(ae);
	while (remaining) {
		if (remaining < sizeof(buff))
			assertEqualInt(remaining,
			    bytes = archive_read_data(a, buff, sizeof(buff)));
		else
			assertEqualInt(sizeof(buff),
			    bytes = archive_read_data(a, buff, sizeof(buff)));
		if (bytes > 0)
			remaining -= bytes;
		else
			break;
	}
	assertEqualInt(0, remaining);

	assertEqualInt(1, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Extract a file compressed with PPMd.
 */
static void
test_ppmd(void)
{
	const char *refname = "test_read_format_7zip_ppmd.7z";
	struct archive_entry *ae;
	struct archive *a;
	size_t remaining;
	ssize_t bytes;
	char buff[1024];

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify regular file1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("ppmd_test.txt", archive_entry_pathname(ae));
	assertEqualInt(1322464589, archive_entry_mtime(ae));
	assertEqualInt(102400, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	remaining = (size_t)archive_entry_size(ae);
	while (remaining) {
		if (remaining < sizeof(buff))
			assertEqualInt(remaining,
			    bytes = archive_read_data(a, buff, sizeof(buff)));
		else
			assertEqualInt(sizeof(buff),
			    bytes = archive_read_data(a, buff, sizeof(buff)));
		if (bytes > 0)
			remaining -= bytes;
		else
			break;
	}
	assertEqualInt(0, remaining);

	assertEqualInt(1, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_ppmd_small_block(void)
{
#ifdef HAVE_LZMA_H
	const char *refname = "test_read_format_7zip_ppmd_small_block.7z";
	const void *block;
	struct archive_entry *ae;
	struct archive *a;
	int64_t offset;
	size_t archive_size, size, total;
	void *archive_data;
	int i, r;

	extract_reference_file(refname);
	archive_data = slurpfile(&archive_size, "%s", refname);
	assert(archive_data != NULL);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    read_open_memory_seek(a, archive_data, archive_size, 1000));

	for (i = 0; i < 4; i++) {
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_next_header(a, &ae));
		assertEqualInt(1024, archive_entry_size(ae));
		total = 0;
		while ((r = archive_read_data_block(a, &block, &size,
		    &offset)) == ARCHIVE_OK)
			total += size;
		assertEqualIntA(a, ARCHIVE_EOF, r);
		assertEqualInt(1024, total);
	}

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(archive_data);
#else
	skipping("lzma tests require liblzma");
#endif
}

static void
test_symname(void)
{
	const char *refname = "test_read_format_7zip_symbolic_name.7z";
	struct archive_entry *ae;
	struct archive *a;
	char buff[128];

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Verify regular file1. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(32, archive_entry_size(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);
	assertEqualInt(32, archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "hellohellohello\nhellohellohello\n", 32);

	/* Verify symbolic-link symlinkfile. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFLNK | 0755), archive_entry_mode(ae));
	assertEqualString("symlinkfile", archive_entry_pathname(ae));
	assertEqualString("file1", archive_entry_symlink(ae));
	assertEqualInt(86401, archive_entry_mtime(ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), 0);

	assertEqualInt(2, archive_file_count(a));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}


DEFINE_TEST(test_read_format_7zip)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/* Extracting with liblzma */
	if (ARCHIVE_OK != archive_read_support_filter_xz(a)) {
		skipping("7zip:lzma decoding is not supported on this "
		    "platform");
	} else {
		test_symname();
		test_extract_all_files("test_read_format_7zip_copy_2.7z");
		test_extract_last_file("test_read_format_7zip_copy_2.7z");
		test_extract_all_files2("test_read_format_7zip_lzma1_lzma2.7z");
		test_bcj("test_read_format_7zip_bcj2_copy_lzma.7z");
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_bzip2)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/* Extracting with libbzip2 */
	if (ARCHIVE_OK != archive_read_support_filter_bzip2(a)) {
		skipping("7zip:bzip2 decoding is not supported on this platform");
	} else {
		test_plain_header("test_read_format_7zip_bzip2.7z");
		test_bcj("test_read_format_7zip_bcj_bzip2.7z");
		test_bcj("test_read_format_7zip_bcj2_bzip2.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_from_fd)
{
	test_copy(1);/* read a 7zip file from a file descriptor. */
}

DEFINE_TEST(test_read_format_7zip_copy)
{
	test_copy(0);
	test_bcj("test_read_format_7zip_bcj_copy.7z");
	test_bcj("test_read_format_7zip_bcj2_copy_1.7z");
	test_bcj("test_read_format_7zip_bcj2_copy_2.7z");
}

DEFINE_TEST(test_read_format_7zip_deflate)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/* Extracting with libz */
	if (ARCHIVE_OK != archive_read_support_filter_gzip(a)) {
		skipping(
		    "7zip:deflate decoding is not supported on this platform");
	} else {
		test_plain_header("test_read_format_7zip_deflate.7z");
		test_bcj("test_read_format_7zip_bcj_deflate.7z");
		test_bcj("test_read_format_7zip_bcj2_deflate.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_zstd)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/* Extracting with libzstd */
	if (ARCHIVE_OK != archive_read_support_filter_zstd(a)) {
		skipping(
		    "7zip:zstd decoding is not supported on this platform");
	} else if (ARCHIVE_OK != archive_read_support_filter_xz(a)) {
		// The directory header entries in the test file uses lzma.
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_extract_all_files_zstd("test_read_format_7zip_zstd.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_zstd_solid)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/* Extracting with libzstd */
	if (ARCHIVE_OK != archive_read_support_filter_zstd(a)) {
		skipping(
		    "7zip:zstd decoding is not supported on this platform");
	} else if (ARCHIVE_OK != archive_read_support_filter_xz(a)) {
		// The directory header entries in the test file uses lzma.
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_extract_all_files_zstd("test_read_format_7zip_solid_zstd.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_zstd_bcj)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/* Extracting with libzstd */
	if (ARCHIVE_OK != archive_read_support_filter_zstd(a)) {
		skipping(
		    "7zip:zstd decoding is not supported on this platform");
	} else {
		test_extract_file_zstd_bcj_nobjc("test_read_format_7zip_zstd_bcj.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_zstd_nobcj)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/* Extracting with libzstd */
	if (ARCHIVE_OK != archive_read_support_filter_zstd(a)) {
		skipping(
		    "7zip:zstd decoding is not supported on this platform");
	} else {
		test_extract_file_zstd_bcj_nobjc("test_read_format_7zip_zstd_nobcj.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_empty)
{
	test_empty_archive();
	test_empty_file();
}

DEFINE_TEST(test_read_format_7zip_lzma1)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/* Extracting with liblzma */
	if (ARCHIVE_OK != archive_read_support_filter_xz(a)) {
		skipping("7zip:lzma decoding is not supported on this "
		    "platform");
	} else {
		test_plain_header("test_read_format_7zip_lzma1.7z");
		test_extract_all_files("test_read_format_7zip_lzma1_2.7z");
		test_extract_last_file("test_read_format_7zip_lzma1_2.7z");
		test_bcj("test_read_format_7zip_bcj_lzma1.7z");
		test_bcj("test_read_format_7zip_bcj2_lzma1_1.7z");
		test_bcj("test_read_format_7zip_bcj2_lzma1_2.7z");
		test_delta_lzma("test_read_format_7zip_delta_lzma1.7z");
		test_delta_lzma("test_read_format_7zip_delta4_lzma1.7z");
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_lzma2)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/* Extracting with liblzma */
	if (ARCHIVE_OK != archive_read_support_filter_xz(a)) {
		skipping("7zip:lzma decoding is not supported on this "
		    "platform");
	} else {
		test_plain_header("test_read_format_7zip_lzma2.7z");
		test_bcj("test_read_format_7zip_bcj_lzma2.7z");
		test_bcj("test_read_format_7zip_bcj2_lzma2_1.7z");
		test_bcj("test_read_format_7zip_bcj2_lzma2_2.7z");
		test_delta_lzma("test_read_format_7zip_delta_lzma2.7z");
		test_delta_lzma("test_read_format_7zip_delta4_lzma2.7z");
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_arm_filter(const char *refname)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[7804];
	uint32_t computed_crc = 0;
	uint32_t expected_crc = 0x355ec4e1;

	assert((a = archive_read_new()) != NULL);

	extract_reference_file(refname);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0755), archive_entry_mode(ae));
	assertEqualString("hw-gnueabihf", archive_entry_pathname(ae));
	assertEqualInt(sizeof(buff), archive_entry_size(ae));
	assertEqualInt(sizeof(buff), archive_read_data(a, buff, sizeof(buff)));
	computed_crc = bitcrc32(computed_crc, buff, sizeof(buff));
	assertEqualInt(computed_crc, expected_crc);

	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_zstd_arm)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_zstd(a)) {
		skipping(
		    "7zip:zstd decoding is not supported on this platform");
	} else {
		test_arm_filter("test_read_format_7zip_zstd_arm.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_lzma2_arm)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_arm_filter("test_read_format_7zip_lzma2_arm.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_lzma1_arm)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_arm_filter("test_read_format_7zip_lzma1_arm.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_armthumb_filter(const char *refname)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[98304];
	uint32_t computed_crc = 0;
	uint32_t expected_crc = 0xa4287015;

	assert((a = archive_read_new()) != NULL);

	extract_reference_file(refname);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("thumb_payload.bin", archive_entry_pathname(ae));
	assertEqualInt(sizeof(buff), archive_entry_size(ae));
	assertEqualInt(sizeof(buff), archive_read_data(a, buff, sizeof(buff)));
	computed_crc = bitcrc32(computed_crc, buff, sizeof(buff));
	assertEqualInt(computed_crc, expected_crc);

	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_deflate_armthumb)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_gzip(a)) {
		skipping(
		    "7zip:deflate decoding is not supported on this platform");
	} else {
		test_armthumb_filter(
		    "test_read_format_7zip_deflate_armthumb.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_lzma1_armthumb)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_armthumb_filter("test_read_format_7zip_lzma1_armthumb.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_ia64_filter(const char *refname)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[98304];
	uint32_t computed_crc = 0;
	uint32_t expected_crc = 0xa0b3ced4;

	assert((a = archive_read_new()) != NULL);

	extract_reference_file(refname);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("ia64_payload.bin", archive_entry_pathname(ae));
	assertEqualInt(sizeof(buff), archive_entry_size(ae));
	assertEqualInt(sizeof(buff), archive_read_data(a, buff, sizeof(buff)));
	computed_crc = bitcrc32(computed_crc, buff, sizeof(buff));
	assertEqualInt(computed_crc, expected_crc);

	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_deflate_ia64)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_gzip(a)) {
		skipping(
		    "7zip:deflate decoding is not supported on this platform");
	} else {
		test_ia64_filter("test_read_format_7zip_deflate_ia64.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_lzma1_ia64)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_ia64_filter("test_read_format_7zip_lzma1_ia64.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_deflate_delta)
{
	const char *refname = "test_read_format_7zip_deflate_delta.7z";
	struct archive *a;
	struct archive_entry *ae;
	char buff[1024];
	ssize_t bytes;
	int64_t total = 0;
	uint32_t computed_crc = 0;
	uint32_t expected_crc = 0x0a49c61b;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_gzip(a)) {
		skipping(
		    "7zip:deflate decoding is not supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return;
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("delta_payload.bin", archive_entry_pathname(ae));
	assertEqualInt(262144, archive_entry_size(ae));
	while ((bytes = archive_read_data(a, buff, sizeof(buff))) > 0) {
		computed_crc = bitcrc32(computed_crc, buff, bytes);
		total += bytes;
	}
	assertEqualInt(0, bytes);
	assertEqualInt(262144, total);
	assertEqualInt(computed_crc, expected_crc);

	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_ppmd)
{
	test_ppmd();
}

DEFINE_TEST(test_read_format_7zip_ppmd_small_block)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/*
	 * Despite the contents of the small block archive being
	 * in PPMd, the directory itself is LZMA-compressed.
	 */
	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_ppmd_small_block();
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_ppmd_filter(const char *refname, const char *pathname, size_t size,
    uint32_t expected_crc)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[16384];
	uint32_t computed_crc;

	assert(size <= sizeof(buff));

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(pathname, archive_entry_pathname(ae));
	assertEqualInt(size, archive_entry_size(ae));
	assertEqualInt(size, archive_read_data(a, buff, size));
	computed_crc = bitcrc32(0, buff, size);
	assertEqualInt(computed_crc, expected_crc);

	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_ppmd_bcj)
{
	test_ppmd_filter("test_read_format_7zip_ppmd_bcj.7z",
	    "ppmd-bcj.bin", 2000, 0x2247d78f);
}

DEFINE_TEST(test_read_format_7zip_ppmd_arm64)
{
	test_ppmd_filter("test_read_format_7zip_ppmd_arm64.7z",
	    "ppmd-arm64.bin", 13066, 0x7b8abc67);
}

static void
test_arm64_filter(const char *refname)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[70368];
	uint32_t computed_crc = 0;
	uint32_t expected_crc = 0xde97d594;

	assert((a = archive_read_new()) != NULL);

	extract_reference_file(refname);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0775), archive_entry_mode(ae));
	assertEqualString("hw-arm64", archive_entry_pathname(ae));
	assertEqualInt(sizeof(buff), archive_entry_size(ae));
	assertEqualInt(sizeof(buff), archive_read_data(a, buff, sizeof(buff)));
	computed_crc = bitcrc32(computed_crc, buff, sizeof(buff));
	assertEqualInt(computed_crc, expected_crc);

	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_lzma2_arm64)
{
#ifdef LZMA_FILTER_ARM64
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_arm64_filter("test_read_format_7zip_lzma2_arm64.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
#else
	skipping("This version of liblzma does not support LZMA_FILTER_ARM64");
#endif
}

DEFINE_TEST(test_read_format_7zip_lzma1_arm64)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_arm64_filter("test_read_format_7zip_lzma1_arm64.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_deflate_arm64)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_gzip(a)) {
		skipping(
		    "7zip:deflate decoding is not supported on this platform");
	} else {
		test_arm64_filter("test_read_format_7zip_deflate_arm64.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_deflate_arm64_unaligned_chunk)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[127536];
	uint32_t computed_crc;

	/*
 	 * This archive is large enough that the compressed stream does not
 	 * fit in the reader's input window, which makes the decompressor
	 * return a chunk whose size is not a multiple of the filter's block
	 * size. The bytes at the end of such a chunk cannot be converted
	 * until the following chunk has been decompressed, so they must be
	 * carried over instead of dropped.
 	 */
	const char *refname = "test_read_format_7zip_deflate_arm64_large.7z";

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_gzip(a)) {
		skipping(
		    "7zip:deflate decoding is not supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return;
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("bcj-arm64.bin", archive_entry_pathname(ae));
	assertEqualInt(sizeof(buff), archive_entry_size(ae));
	assertEqualInt(sizeof(buff), archive_read_data(a, buff, sizeof(buff)));
	computed_crc = bitcrc32(0, buff, sizeof(buff));
	assertEqualInt(computed_crc, 0xfef87379);

	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_deflate_arm_arm64)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff_arm[7804];
	char buff_arm64[70368];
	uint32_t computed_crc;
	const char *refname = "test_read_format_7zip_deflate_arm_arm64.7z";

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_gzip(a)) {
		skipping(
		    "7zip:deflate decoding is not supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return;
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	/* First folder: ARM+Deflate. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("hw-gnueabihf", archive_entry_pathname(ae));
	assertEqualInt(sizeof(buff_arm), archive_entry_size(ae));
	assertEqualInt(sizeof(buff_arm),
	    archive_read_data(a, buff_arm, sizeof(buff_arm)));
	computed_crc = bitcrc32(0, buff_arm, sizeof(buff_arm));
	assertEqualInt(computed_crc, 0x355ec4e1);

	/* Second folder: ARM64+Deflate. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("hw-arm64", archive_entry_pathname(ae));
	assertEqualInt(sizeof(buff_arm64), archive_entry_size(ae));
	assertEqualInt(sizeof(buff_arm64),
	    archive_read_data(a, buff_arm64, sizeof(buff_arm64)));
	computed_crc = bitcrc32(0, buff_arm64, sizeof(buff_arm64));
	assertEqualInt(computed_crc, 0xde97d594);

	assertEqualInt(2, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_win_attrib)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return;
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	// This archive has four files and four directories:
	// * hidden directory
	// * readonly directory
	// * regular directory
	// * system directory
	// * regular "archive" file
	// * hidden file
	// * readonly file
	// * system file
	const char *refname = "test_read_format_7zip_win_attrib.7z";
	extract_reference_file(refname);

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	struct archive_entry *ae;

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("hidden_dir/", archive_entry_pathname(ae));
	assertEqualInt((AE_IFDIR | 0755), archive_entry_mode(ae));
	assertEqualString("hidden", archive_entry_fflags_text(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("readonly_dir/", archive_entry_pathname(ae));
	assertEqualInt((AE_IFDIR | 0555), archive_entry_mode(ae));
	assertEqualString("rdonly", archive_entry_fflags_text(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("regular_dir/", archive_entry_pathname(ae));
	assertEqualInt((AE_IFDIR | 0755), archive_entry_mode(ae));
	assertEqualString(NULL, archive_entry_fflags_text(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("system_dir/", archive_entry_pathname(ae));
	assertEqualInt((AE_IFDIR | 0755), archive_entry_mode(ae));
	assertEqualString("system", archive_entry_fflags_text(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("archive_file.txt", archive_entry_pathname(ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString(NULL, archive_entry_fflags_text(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("hidden_file.txt", archive_entry_pathname(ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("hidden", archive_entry_fflags_text(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("readonly_file.txt", archive_entry_pathname(ae));
	assertEqualInt((AE_IFREG | 0444), archive_entry_mode(ae));
	assertEqualString("rdonly", archive_entry_fflags_text(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("system_file.txt", archive_entry_pathname(ae));
	assertEqualInt((AE_IFREG | 0644), archive_entry_mode(ae));
	assertEqualString("system", archive_entry_fflags_text(ae));


	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_sfx_pe)
{
	/*
	 * This is a regular 7z SFX PE file
	 * created by 7z tool v22.01 on Windows 64-bit
	 */
	struct archive *a;
	struct archive_entry *ae;
	int bs = 10240;
	char buff[32];
	const char reffile[] = "test_read_format_7zip_sfx_pe.exe";
	const char test_txt[] = "123";
	int size = sizeof(test_txt) - 1;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		extract_reference_file(reffile);
		assertA(0 == archive_read_support_filter_all(a));
		assertA(0 == archive_read_support_format_all(a));
		assertA(0 == archive_read_open_filename(a, reffile, bs));

		assertA(0 == archive_read_next_header(a, &ae));
		assertEqualString("test.txt.txt", archive_entry_pathname(ae));

		assertA(size == archive_read_data(a, buff, size));
		assertEqualMem(buff, test_txt, size);

		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_sfx_modified_pe)
{
	/*
	 * This test simulates a modified 7z SFX PE
	 * the compressed data in the SFX file is still stored as PE overlay
	 * but the decompressor code is replaced
	 */
	struct archive *a;
	struct archive_entry *ae;
	int bs = 10240;
	char buff[32];
	const char reffile[] = "test_read_format_7zip_sfx_modified_pe.exe";
	const char test_txt[] = "123";
	int size = sizeof(test_txt) - 1;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		extract_reference_file(reffile);
		assertA(0 == archive_read_support_filter_all(a));
		assertA(0 == archive_read_support_format_all(a));
		assertA(0 == archive_read_open_filename(a, reffile, bs));

		assertA(0 == archive_read_next_header(a, &ae));
		assertEqualString("test.txt.txt", archive_entry_pathname(ae));

		assertA(size == archive_read_data(a, buff, size));
		assertEqualMem(buff, test_txt, size);

		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_sfx_elf)
{
	/*
	 * This is a regular 7z SFX ELF file
	 * created by 7z tool v16.02 on Ubuntu
	 */
	struct archive *a;
	struct archive_entry *ae;
	int bs = 10240;
	char buff[32];
	const char reffile[] = "test_read_format_7zip_sfx_elf.elf";
	const char test_txt[] = "123";
	int size = sizeof(test_txt) - 1;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		extract_reference_file(reffile);
		assertA(0 == archive_read_support_filter_all(a));
		assertA(0 == archive_read_support_format_all(a));
		assertA(0 == archive_read_open_filename(a, reffile, bs));

		assertA(0 == archive_read_next_header(a, &ae));
		assertEqualString("test.txt.txt", archive_entry_pathname(ae));

		assertA(size == archive_read_data(a, buff, size));
		assertEqualMem(buff, test_txt, size);

		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_sfx_boundary_empty_7z(const char *refname)
{
	struct archive_entry *ae = NULL;
	struct archive *a;
	const int bs = 10240;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, bs));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(0, archive_file_count(a));
	assertEqualIntA(a, ARCHIVE_FORMAT_7ZIP, archive_format(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_sfx_boundary_pe)
{
	/*
	 * The embedded empty 7z archive starts at the final exact
	 * 32-byte scan window after the PE overlay offset.
	 */
	test_sfx_boundary_empty_7z(
	    "test_read_format_7zip_sfx_boundary_pe.exe");
}

DEFINE_TEST(test_read_format_7zip_sfx_boundary_elf)
{
	/*
	 * The ELF section string table contains "\0.data\0", so
	 * ".data\0" starts at the final valid string-table position.
	 */
	test_sfx_boundary_empty_7z(
	    "test_read_format_7zip_sfx_boundary_elf.elf");
}

/*
 * A truncated ELF 64-bit MSB file.
 */
DEFINE_TEST(test_read_format_7zip_sfx_elf64trunc)
{
	const char *reffile = "test_read_format_7zip_sfx_elf64trunc.elf";
	struct archive_entry *ae;
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	extract_reference_file(reffile);
	assertEqualIntA(a, ARCHIVE_FATAL,
	    archive_read_open_filename(a, reffile, 10240));

	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualInt(0, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_extract_second)
{
	struct archive *a;
	char buffer[256];

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		return;
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	/*
	 * The test archive has two files: first.txt which is a 65,536 file (the
	 * size of the uncompressed buffer), and second.txt which has contents
	 * we will validate. This test ensures we can skip first.txt and still
	 * be able to read the contents of second.txt
	 */
	const char *refname = "test_read_format_7zip_extract_second.7z";
	extract_reference_file(refname);

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	struct archive_entry *ae;

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("first.txt", archive_entry_pathname(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("second.txt", archive_entry_pathname(ae));

	assertEqualInt(23, archive_read_data(a, buffer, sizeof(buffer)));
	assertEqualMem("This is from second.txt", buffer, 23);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_riscv_filter(const char *refname)
{
	struct archive *a;
	struct archive_entry *ae;
	char buff[8488];
	uint32_t computed_crc = 0;
	uint32_t expected_crc = 0xf7ed24e7;

	assert((a = archive_read_new()) != NULL);

	extract_reference_file(refname);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0775), archive_entry_mode(ae));
	assertEqualString("hw-riscv64", archive_entry_pathname(ae));
	assertEqualIntA(a, sizeof(buff), archive_entry_size(ae));
	assertEqualIntA(a, sizeof(buff), archive_read_data(a, buff, sizeof(buff)));

	computed_crc = bitcrc32(computed_crc, buff, sizeof(buff));
	assertEqualInt(computed_crc, expected_crc);

	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_lzma2_riscv)
{
#ifdef LZMA_FILTER_RISCV
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping("7zip:lzma decoding is not supported on this platform");
	} else if (!lzma_filter_decoder_is_supported(LZMA_FILTER_RISCV)) {
		skipping("liblzma does not support RISC-V BCJ filter at runtime");
	} else {
		test_riscv_filter("test_read_format_7zip_lzma2_riscv.7z");
	}
	archive_read_free(a);
#else
	skipping("This version of liblzma does not support LZMA_FILTER_RISCV");
#endif
}

DEFINE_TEST(test_read_format_7zip_lzma1_riscv)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping("7zip:lzma decoding is not supported on this platform");
	} else {
		test_riscv_filter("test_read_format_7zip_lzma1_riscv.7z");
	}
	archive_read_free(a);
}

DEFINE_TEST(test_read_format_7zip_deflate_riscv)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_gzip(a)) {
		skipping(
		    "7zip:deflate decoding is not supported on this platform");
	} else {
		test_riscv_filter("test_read_format_7zip_deflate_riscv.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_sparc_filter(const char *refname)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t expected_entry_size = 1053016;
	char *buff = malloc(expected_entry_size);
	uint32_t computed_crc = 0;
	uint32_t expected_crc = 0x6b5b364d;

	assert((a = archive_read_new()) != NULL);

	extract_reference_file(refname);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0775), archive_entry_mode(ae));
	assertEqualString("hw-sparc64", archive_entry_pathname(ae));
	assertEqualInt(expected_entry_size, archive_entry_size(ae));
	assertEqualInt(expected_entry_size, archive_read_data(a, buff, expected_entry_size));

	computed_crc = bitcrc32(computed_crc, buff, expected_entry_size);
	assertEqualInt(computed_crc, expected_crc);

	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
}

DEFINE_TEST(test_read_format_7zip_lzma2_sparc)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_sparc_filter("test_read_format_7zip_lzma2_sparc.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_lzma1_sparc)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_sparc_filter("test_read_format_7zip_lzma1_sparc.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_zstd_sparc)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_zstd(a)) {
		skipping(
		    "7zip:zstd decoding is not supported on this platform");
	} else {
		test_sparc_filter("test_read_format_7zip_zstd_sparc.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_powerpc_filter(const char *refname)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t expected_entry_size = 68340;
	char *buff = malloc(expected_entry_size);
	uint32_t computed_crc = 0;
	uint32_t expected_crc = 0x71fb03c9;

	assert((a = archive_read_new()) != NULL);

	extract_reference_file(refname);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	assertEqualIntA(a, ARCHIVE_OK,
		archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualInt((AE_IFREG | 0775), archive_entry_mode(ae));
	assertEqualString("hw-powerpc", archive_entry_pathname(ae));
	assertEqualInt(expected_entry_size, archive_entry_size(ae));
	assertEqualInt(expected_entry_size, archive_read_data(a, buff, expected_entry_size));

	computed_crc = bitcrc32(computed_crc, buff, expected_entry_size);
	assertEqualInt(computed_crc, expected_crc);

	assertEqualInt(1, archive_file_count(a));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
}

DEFINE_TEST(test_read_format_7zip_deflate_powerpc)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_gzip(a)) {
		skipping(
		    "7zip:deflate decoding is not supported on this platform");
	} else {
		test_powerpc_filter("test_read_format_7zip_deflate_powerpc.7z");
	}
  
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_lzma2_powerpc)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_powerpc_filter("test_read_format_7zip_lzma2_powerpc.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * A degenerate 7-Zip header can leave __archive_read_filter_ahead() with an
 * empty client buffer and an empty copy buffer, that is with
 * f->client_next == NULL and f->avail == 0.  The unconditional
 * "f->client_next -= f->avail" was then a zero offset applied to a null
 * pointer; a UBSan build aborts on it.  The observable behaviour is
 * unchanged, so only a sanitizer build catches a regression here.
 */
DEFINE_TEST(test_read_format_7zip_null_client_next)
{
	const char *refname = "test_read_format_7zip_null_client_next.7z";
	struct archive *a;
	struct archive_entry *ae;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_7zip_lzma1_powerpc)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	if (ARCHIVE_OK != archive_read_support_filter_lzma(a)) {
		skipping(
		    "7zip:lzma decoding is not supported on this platform");
	} else {
		test_powerpc_filter("test_read_format_7zip_lzma1_powerpc.7z");
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
