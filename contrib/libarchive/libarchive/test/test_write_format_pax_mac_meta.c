/*-
 * Copyright (c) 2026 Tobias Stoeckmann
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
#include "archive_entry.h"
#include "test.h"

DEFINE_TEST(test_write_format_pax_mac_meta)
{
	struct archive_entry *ae;
	struct archive *a;
	char *buff;
	size_t buffsize = 1000000;
	size_t size, used;

	/* Create a new in-memory pax archive. */
	a = archive_write_new();
	assert(a != NULL);
	buff = malloc(buffsize);
	assert(buff != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_pax(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	/* Mac metadata cannot be added to root directory. */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "/");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_copy_mac_metadata(ae, "root-meta", sizeof("root-meta"));
	assertEqualIntA(a, ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Add directory entry "dir1" with mac metadata. */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "dir1");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_copy_mac_metadata(ae, "dir1-meta", sizeof("dir1-meta"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Add sub directory entry "dir1/sub1" with mac metadata. */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "dir1/sub1");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_copy_mac_metadata(ae, "sub1-meta", sizeof("sub1-meta"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Add file entry "dir1/sub1//file1" with mac metadata. */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "dir1/sub1//file");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, 0);
	archive_entry_copy_mac_metadata(ae, "file-meta", sizeof("file-meta"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Add directory entry "dir2///" with mac metadata. */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "dir2///");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_copy_mac_metadata(ae, "dir2-meta", sizeof("dir2-meta"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Add directory entry "/dir3" with mac metadata. */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "/dir3");
	archive_entry_set_mode(ae, AE_IFDIR | 0755);
	archive_entry_copy_mac_metadata(ae, "dir3-meta", sizeof("dir3-meta"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Mac metadata cannot be added to empty pathname. */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_copy_mac_metadata(ae, "empty-meta", sizeof("empty-meta"));
	assertEqualIntA(a, ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Close archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Read archive, including mac metadata. */
	a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_format_option(a, "tar", "mac-ext", "1"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, buff, used));

	/* Read "dir1". */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("dir1/", archive_entry_pathname(ae));
	assert((S_IFDIR | 0755) == archive_entry_mode(ae));
	assertEqualMem("dir1-meta", archive_entry_mac_metadata(ae, &size),
	    sizeof("dir1-meta"));
	assertEqualInt(sizeof("dir1-meta"), size);

	/* Read "dir1/sub1". */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("dir1/sub1/", archive_entry_pathname(ae));
	assert((S_IFDIR | 0755) == archive_entry_mode(ae));
	assertEqualMem("sub1-meta", archive_entry_mac_metadata(ae, &size),
	    sizeof("sub1-meta"));
	assertEqualInt(sizeof("sub1-meta"), size);

	/* Read "dir1/sub1//file". */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("dir1/sub1//file", archive_entry_pathname(ae));
	assert((S_IFREG | 0644) == archive_entry_mode(ae));
	assertEqualMem("file-meta", archive_entry_mac_metadata(ae, &size),
	    sizeof("file-meta"));
	assertEqualInt(sizeof("file-meta"), size);

	/* Read dir2///". */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("dir2///", archive_entry_pathname(ae));
	assert((S_IFDIR | 0755) == archive_entry_mode(ae));
	assertEqualMem("dir2-meta", archive_entry_mac_metadata(ae, &size),
	    sizeof("dir2-meta"));
	assertEqualInt(sizeof("dir2-meta"), size);

	/* Read /dir3". */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("/dir3/", archive_entry_pathname(ae));
	assert((S_IFDIR | 0755) == archive_entry_mode(ae));
	assertEqualMem("dir3-meta", archive_entry_mac_metadata(ae, &size),
	    sizeof("dir3-meta"));
	assertEqualInt(sizeof("dir3-meta"), size);

	/* Verify the end of the archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Close archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
}
