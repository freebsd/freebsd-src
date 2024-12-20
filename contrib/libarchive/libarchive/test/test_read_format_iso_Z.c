/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

static void
test1(void)
{
	struct archive_entry *ae;
	struct archive *a;
	const char *name = "test_read_format_iso.iso.Z";

	extract_reference_file(name);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, name, 512));

	/* Root directory */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualIntA(a, 1131434684, archive_entry_atime(ae));
	assertEqualIntA(a, 0, archive_entry_birthtime(ae));
	assertEqualIntA(a, 1131434684, archive_entry_ctime(ae));
	assertEqualIntA(a, 0, archive_entry_dev(ae));
	assertEqualIntA(a, AE_IFDIR, archive_entry_filetype(ae));
	assertEqualIntA(a, 0, archive_entry_gid(ae));
	assertEqualStringA(a, NULL, archive_entry_gname(ae));
	assertEqualIntA(a, 0, archive_entry_ino(ae));
	assertEqualIntA(a, AE_IFDIR | 0700, archive_entry_mode(ae));
	assertEqualIntA(a, 1131434684, archive_entry_mtime(ae));
	assertEqualIntA(a, 2, archive_entry_nlink(ae));
	assertEqualStringA(a, ".", archive_entry_pathname(ae));
	assertEqualIntA(a, 0700, archive_entry_perm(ae));
	assertEqualIntA(a, 2048, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_entry_uid(ae));
	assertEqualStringA(a, NULL, archive_entry_uname(ae));

	assertEqualIntA(a, ARCHIVE_EOF,
	    archive_read_next_header(a, &ae));
	assertEqualInt(1, archive_file_count(a));
	assertEqualInt(archive_filter_code(a, 0),
	    ARCHIVE_FILTER_COMPRESS);
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_ISO9660);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_small(const char *name)
{
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(name);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, name, 512));

	/* Root directory */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualString(".", archive_entry_pathname(ae));
	assertEqualInt(0, archive_entry_atime_is_set(ae));
	assertEqualInt(0, archive_entry_atime(ae));
	assertEqualInt(0, archive_entry_birthtime_is_set(ae));
	assertEqualInt(0, archive_entry_birthtime(ae));
	assertEqualInt(0, archive_entry_ctime_is_set(ae));
	assertEqualInt(0, archive_entry_ctime(ae));
	assertEqualIntA(a, 0, archive_entry_dev(ae));
	assertEqualIntA(a, AE_IFDIR, archive_entry_filetype(ae));
	assertEqualIntA(a, 0, archive_entry_gid(ae));
	assertEqualStringA(a, NULL, archive_entry_gname(ae));
	assertEqualIntA(a, 0, archive_entry_ino(ae));
	assertEqualIntA(a, AE_IFDIR | 0700, archive_entry_mode(ae));
	assertEqualInt(0, archive_entry_mtime_is_set(ae));
	assertEqualInt(0, archive_entry_mtime(ae));
	assertEqualIntA(a, 4, archive_entry_nlink(ae));
	assertEqualIntA(a, 0700, archive_entry_perm(ae));
	assertEqualIntA(a, 2048, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_entry_uid(ae));
	assertEqualStringA(a, NULL, archive_entry_uname(ae));

	/* Directory "A" */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualString("A", archive_entry_pathname(ae));
	assertEqualIntA(a, 1313381406, archive_entry_atime(ae));
	assertEqualIntA(a, 0, archive_entry_birthtime(ae));
	assertEqualIntA(a, 1313381406, archive_entry_ctime(ae));
	assertEqualIntA(a, 0, archive_entry_dev(ae));
	assertEqualIntA(a, AE_IFDIR, archive_entry_filetype(ae));
	assertEqualIntA(a, 0, archive_entry_gid(ae));
	assertEqualStringA(a, NULL, archive_entry_gname(ae));
	assertEqualIntA(a, 0, archive_entry_ino(ae));
	assertEqualIntA(a, AE_IFDIR | 0700, archive_entry_mode(ae));
	assertEqualIntA(a, 1313381406, archive_entry_mtime(ae));
	assertEqualIntA(a, 2, archive_entry_nlink(ae));
	assertEqualIntA(a, 0700, archive_entry_perm(ae));
	assertEqualIntA(a, 2048, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_entry_uid(ae));
	assertEqualStringA(a, NULL, archive_entry_uname(ae));

	/* File "A/B" */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualString("A/B", archive_entry_pathname(ae));
	assertEqualIntA(a, 1313381406, archive_entry_atime(ae));
	assertEqualIntA(a, 0, archive_entry_birthtime(ae));
	assertEqualIntA(a, 1313381406, archive_entry_ctime(ae));
	assertEqualIntA(a, 0, archive_entry_dev(ae));
	assertEqualIntA(a, AE_IFREG, archive_entry_filetype(ae));
	assertEqualIntA(a, 0, archive_entry_gid(ae));
	assertEqualStringA(a, NULL, archive_entry_gname(ae));
	assertEqualIntA(a, 0, archive_entry_ino(ae));
	assertEqualIntA(a, AE_IFREG | 0400, archive_entry_mode(ae));
	assertEqualIntA(a, 1313381406, archive_entry_mtime(ae));
	assertEqualIntA(a, 1, archive_entry_nlink(ae));
	assertEqualIntA(a, 0400, archive_entry_perm(ae));
	assertEqualIntA(a, 6, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_entry_uid(ae));
	assertEqualStringA(a, NULL, archive_entry_uname(ae));
	/* TODO: Verify that file contents are "hello\n" */

	/* Directory "C" */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualString("C", archive_entry_pathname(ae));
	assertEqualIntA(a, 1313381411, archive_entry_atime(ae));
	assertEqualIntA(a, 0, archive_entry_birthtime(ae));
	assertEqualIntA(a, 1313381411, archive_entry_ctime(ae));
	assertEqualIntA(a, 0, archive_entry_dev(ae));
	assertEqualIntA(a, AE_IFDIR, archive_entry_filetype(ae));
	assertEqualIntA(a, 0, archive_entry_gid(ae));
	assertEqualStringA(a, NULL, archive_entry_gname(ae));
	assertEqualIntA(a, 0, archive_entry_ino(ae));
	assertEqualIntA(a, AE_IFDIR | 0700, archive_entry_mode(ae));
	assertEqualIntA(a, 1313381411, archive_entry_mtime(ae));
	assertEqualIntA(a, 2, archive_entry_nlink(ae));
	assertEqualIntA(a, 0700, archive_entry_perm(ae));
	assertEqualIntA(a, 2048, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_entry_uid(ae));
	assertEqualStringA(a, NULL, archive_entry_uname(ae));

	/* File "C/D" */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualString("C/D", archive_entry_pathname(ae));
	assertEqualIntA(a, 1313381411, archive_entry_atime(ae));
	assertEqualIntA(a, 0, archive_entry_birthtime(ae));
	assertEqualIntA(a, 1313381411, archive_entry_ctime(ae));
	assertEqualIntA(a, 0, archive_entry_dev(ae));
	assertEqualIntA(a, AE_IFREG, archive_entry_filetype(ae));
	assertEqualIntA(a, 0, archive_entry_gid(ae));
	assertEqualStringA(a, NULL, archive_entry_gname(ae));
	assertEqualIntA(a, 0, archive_entry_ino(ae));
	assertEqualIntA(a, AE_IFREG | 0400, archive_entry_mode(ae));
	assertEqualIntA(a, 1313381411, archive_entry_mtime(ae));
	assertEqualIntA(a, 1, archive_entry_nlink(ae));
	assertEqualIntA(a, 0400, archive_entry_perm(ae));
	assertEqualIntA(a, 6, archive_entry_size(ae));
	assertEqualIntA(a, 0, archive_entry_uid(ae));
	assertEqualStringA(a, NULL, archive_entry_uname(ae));
	/* TODO: Verify that file contents are "hello\n" */

	/* Final statistics */
	assertEqualIntA(a, ARCHIVE_EOF,
	    archive_read_next_header(a, &ae));
	assertEqualInt(5, archive_file_count(a));
	assertEqualInt(archive_filter_code(a, 0),
	    ARCHIVE_FILTER_COMPRESS);
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_ISO9660);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_iso_Z)
{
	test1();
	/* A very small ISO image with a variety of contents. */
	test_small("test_read_format_iso_2.iso.Z");
	/* As above, but with a non-standard 68-byte root directory in the PVD */
	test_small("test_read_format_iso_3.iso.Z");
}
