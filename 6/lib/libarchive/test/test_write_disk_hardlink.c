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
__FBSDID("$FreeBSD$");

#define UMASK 022

/*
 * Exercise hardlink recreation.
 *
 * File permissions are chosen so that the authoritive entry
 * has the correct permission and the non-authoritive versions
 * are just writeable files.
 */
DEFINE_TEST(test_write_disk_hardlink)
{
#if ARCHIVE_VERSION_STAMP < 1009000
	skipping("archive_write_disk_hardlink tests");
#else
	static const char data[]="abcdefghijklmnopqrstuvwxyz";
	struct archive *ad;
	struct archive_entry *ae;
	struct stat st, st2;

	/* Force the umask to something predictable. */
	umask(UMASK);

	/* Write entries to disk. */
	assert((ad = archive_write_disk_new()) != NULL);

	/*
	 * First, use a tar-like approach; a regular file, then
	 * a separate "hardlink" entry.
	 */

	/* Regular file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link1a");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, sizeof(data));
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(sizeof(data), archive_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	/* Link. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link1b");
	archive_entry_set_mode(ae, S_IFREG | 0600);
	archive_entry_set_size(ae, 0);
	archive_entry_copy_hardlink(ae, "link1a");
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(0, archive_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	/*
	 * Second, try an old-cpio-like approach; a regular file, then
	 * another identical one (which has been marked hardlink).
	 */

	/* Regular file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link2a");
	archive_entry_set_mode(ae, S_IFREG | 0600);
	archive_entry_set_size(ae, sizeof(data));
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(sizeof(data), archive_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	/* Link. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link2b");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, sizeof(data));
	archive_entry_copy_hardlink(ae, "link2a");
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(sizeof(data), archive_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	/*
	 * Finally, try a new-cpio-like approach, where the initial
	 * regular file is empty and the hardlink has the data.
	 */

	/* Regular file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link3a");
	archive_entry_set_mode(ae, S_IFREG | 0600);
	archive_entry_set_size(ae, 0);
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
#if ARCHIVE_VERSION_NUMBER < 3000000
	assertEqualInt(ARCHIVE_WARN, archive_write_data(ad, data, 1));
#else
	assertEqualInt(-1, archive_write_data(ad, data, 1));
#endif
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	archive_entry_free(ae);

	/* Link. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "link3b");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, sizeof(data));
	archive_entry_copy_hardlink(ae, "link3a");
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualInt(sizeof(data), archive_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
	archive_entry_free(ae);
#if ARCHIVE_VERSION_NUMBER < 2000000
	archive_write_finish(ad);
#else
	assertEqualInt(0, archive_write_finish(ad));
#endif

	/* Test the entries on disk. */
	assert(0 == stat("link1a", &st));
	assertEqualInt(st.st_mode, (S_IFREG | 0755) & ~UMASK);
	assertEqualInt(st.st_size, sizeof(data));
	assertEqualInt(st.st_nlink, 2);

	assert(0 == stat("link1b", &st2));
	assertEqualInt(st2.st_mode, (S_IFREG | 0755) & ~UMASK);
	assertEqualInt(st2.st_size, sizeof(data));
	assertEqualInt(st2.st_nlink, 2);
	assertEqualInt(st.st_ino, st2.st_ino);
	assertEqualInt(st.st_dev, st2.st_dev);

	assert(0 == stat("link2a", &st));
	assertEqualInt(st.st_mode, (S_IFREG | 0755) & ~UMASK);
	assertEqualInt(st.st_size, sizeof(data));
	assertEqualInt(st.st_nlink, 2);

	assert(0 == stat("link2b", &st2));
	assertEqualInt(st2.st_mode, (S_IFREG | 0755) & ~UMASK);
	assertEqualInt(st2.st_size, sizeof(data));
	assertEqualInt(st2.st_nlink, 2);
	assertEqualInt(st.st_ino, st2.st_ino);
	assertEqualInt(st.st_dev, st2.st_dev);

	assert(0 == stat("link3a", &st));
	assertEqualInt(st.st_mode, (S_IFREG | 0755) & ~UMASK);
	assertEqualInt(st.st_size, sizeof(data));
	assertEqualInt(st.st_nlink, 2);

	assert(0 == stat("link3b", &st2));
	assertEqualInt(st2.st_mode, (S_IFREG | 0755) & ~UMASK);
	assertEqualInt(st2.st_size, sizeof(data));
	assertEqualInt(st2.st_nlink, 2);
	assertEqualInt(st.st_ino, st2.st_ino);
	assertEqualInt(st.st_dev, st2.st_dev);
#endif
}
