/*-
 * Copyright (c) 2026 krishna28238-arch
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
 * Directories nested deeper than eight levels are relocated into the
 * rr_moved directory when the archive is closed (Rock Ridge, RRIP
 * 4.1.5).  A user-supplied entry called "rr_moved" is adopted as the
 * relocation target.  When that entry is a regular file, relocated
 * subtrees used to be attached to a non-directory entry, which made
 * the writer produce a broken image while reporting success and leak
 * the whole relocated subtree.
 */
DEFINE_TEST(test_write_format_iso9660_rr_moved_notdir)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buff;
	size_t buffsize = 256 * 2048;
	size_t used;

	buff = malloc(buffsize);
	assert(buff != NULL);
	if (buff == NULL)
		return;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_iso9660(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_per_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_in_last_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, buffsize, &used));

	/* A regular file called rr_moved at the root of the archive. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "rr_moved");
	archive_entry_set_size(ae, 0);
	archive_entry_set_mode(ae, S_IFREG | 0644);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* A path nested more than eight levels deep. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae,
	    "d1/d2/d3/d4/d5/d6/d7/d8/d9/d10/d11/d12/file.txt");
	archive_entry_set_size(ae, 4);
	archive_entry_set_mode(ae, S_IFREG | 0644);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 4, archive_write_data(a, "body", 4));
	archive_entry_free(ae);

	/* Relocating into the file cannot work; the close has to say so
	 * instead of writing a broken image and leaking the tree. */
	assertEqualIntA(a, ARCHIVE_FATAL, archive_write_close(a));
	if (archive_error_string(a) != NULL)
		assert(NULL != strstr(archive_error_string(a),
		    "rr_moved"));

	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	free(buff);
}

/*
 * A user-supplied rr_moved directory is still adopted and receives
 * the relocated subtrees.
 */
DEFINE_TEST(test_write_format_iso9660_rr_moved_dir)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buff;
	size_t buffsize = 256 * 2048;
	size_t used;

	buff = malloc(buffsize);
	assert(buff != NULL);
	if (buff == NULL)
		return;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_iso9660(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_per_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_in_last_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, buffsize, &used));

	/* A directory called rr_moved supplied by the caller. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "rr_moved");
	archive_entry_set_size(ae, 0);
	archive_entry_set_mode(ae, S_IFDIR | 0755);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* A path nested more than eight levels deep. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae,
	    "d1/d2/d3/d4/d5/d6/d7/d8/d9/d10/d11/d12/file.txt");
	archive_entry_set_size(ae, 4);
	archive_entry_set_mode(ae, S_IFREG | 0644);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 4, archive_write_data(a, "body", 4));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));

	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	free(buff);
}
