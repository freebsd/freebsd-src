/*-
 * Copyright (c) 2021 Martin Matuska
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
 * Test fixup entries don't follow symlinks
 */
DEFINE_TEST(test_write_disk_fixup)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	skipping("Skipping test on Windows");
#else
	struct archive *ad;
	struct archive_entry *ae;
	int r;

	if (!canSymlink()) {
		skipping("Symlinks not supported");
		return;
	}

	/* Write entries to disk. */
	assert((ad = archive_write_disk_new()) != NULL);

	/*
	 * Create a file
	 */
	assertMakeFile("victim", 0600, "a");

	/*
	 * Create a directory and a symlink with the same name
	 */

	/* Directory: dir */
        assert((ae = archive_entry_new()) != NULL);
        archive_entry_copy_pathname(ae, "dir");
        archive_entry_set_mode(ae, AE_IFDIR | 0606);
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
        archive_entry_free(ae);

	/* Symbolic Link: dir -> foo */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_mode(ae, AE_IFLNK | 0777);
	archive_entry_set_size(ae, 0);
	archive_entry_copy_symlink(ae, "victim");
	assertEqualIntA(ad, 0, r = archive_write_header(ad, ae));
	if (r >= ARCHIVE_WARN)
		assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));

	/* Test the entries on disk. */
	assertIsSymlink("dir", "victim", 0);
	assertFileMode("victim", 0600);
#endif
}
