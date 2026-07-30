/*-SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Dustin L. Howett
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
 * Writing a file to zisofs which is near the 4 GiB limit triggered an
 * overflow and wraparound in calculating how much space we needed to
 * store compressed block pointers, which led to an out-of-bounds write
 * to the heap.
 *
 * This test can only fail under ASan.
 */
DEFINE_TEST(test_write_format_iso9660_zisofs_overflow)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *inbuff, *arcbuff;
	size_t buffsize = 2048 * 288, writesize = 1048576, entrysize = UINT32_MAX - 32000, used;
	int r;

	/* ISO9660 format: Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, 0, archive_write_set_format_iso9660(a));
	assertEqualIntA(a, 0, archive_write_add_filter_none(a));
	r = archive_write_set_option(a, NULL, "zisofs", "1");
	if (r == ARCHIVE_FATAL) {
		skipping("zisofs option not supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_write_free(a));
		return;
	}

	arcbuff = malloc(buffsize);
	assert(arcbuff != NULL);
	if (arcbuff == NULL)
		return;

	inbuff = calloc(writesize, 1);
	assert(inbuff != NULL);
	if (inbuff == NULL) {
		free(arcbuff);
		return;
	}

	assertEqualIntA(a, 0, archive_write_set_option(a, NULL, "pad", NULL));
	assertEqualIntA(a, 0, archive_write_open_memory(a, arcbuff, buffsize, &used));

	/*
	 * "file1" is almost exactly 4 GiB; it's enough to trigger a block pointer counting
	 * overflow in zisofs.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_atime(ae, 2, 20);
	archive_entry_set_birthtime(ae, 3, 30);
	archive_entry_set_ctime(ae, 4, 40);
	archive_entry_set_mtime(ae, 5, 50);
	archive_entry_copy_pathname(ae, "file1");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, entrysize);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	while (entrysize > writesize) {
		assertEqualIntA(a, writesize, archive_write_data(a, inbuff, writesize));
		entrysize -= writesize;
	}
	/* remainder */
	if (0 != (writesize = entrysize))
		assertEqualIntA(a, writesize, archive_write_data(a, inbuff, writesize));

	/* Close out the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	failure("The ISO image size should be 589824 bytes.");
	assertEqualInt(used, 2048 * 288);

	/*
	 * Read ISO image (basic sanity check).
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, 0, archive_read_support_format_all(a));
	assertEqualIntA(a, 0, archive_read_support_filter_all(a));
	assertEqualIntA(a, 0, archive_read_open_memory(a, arcbuff, used));

	/*
	 * Skip the root directory
	 */
	assertEqualIntA(a, 0, archive_read_next_header(a, &ae));

	/*
	 * file1
	 */
	assertEqualIntA(a, 0, archive_read_next_header(a, &ae));

	/*
	 * Verify the end of the archive.
	 */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(arcbuff);
	free(inbuff);
}
