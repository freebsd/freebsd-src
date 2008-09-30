/*-
 * Copyright (c) 2003-2008 Tim Kientzle
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
 * Exercise time restores in archive_write_disk(), including
 * correct handling of omitted time values.
 * On FreeBSD, we also test birthtime and high-res time restores.
 */

DEFINE_TEST(test_write_disk_times)
{
	struct archive *a;
	struct archive_entry *ae;
	struct stat st;
 	time_t now = time(NULL);

	/* Create an archive_write_disk object. */
	assert((a = archive_write_disk_new()) != NULL);
	assertEqualInt(ARCHIVE_OK,
	    archive_write_disk_set_options(a, ARCHIVE_EXTRACT_TIME));

	/*
	 * Easy case: mtime and atime both specified.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file1");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_atime(ae, 123456, 0);
	archive_entry_set_mtime(ae, 234567, 0);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(ae);
	/* Verify */
	assertEqualInt(0, stat("file1", &st));
	assertEqualInt(123456, st.st_atime);
	assertEqualInt(234567, st.st_mtime);

	/*
	 * mtime specified, but not atime
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file2");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_mtime(ae, 234567, 0);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(ae);
	/* Verify: Current atime and mtime as specified. */
	assertEqualInt(0, stat("file2", &st));
	assertEqualInt(234567, st.st_mtime);
	failure("now: %ld st.st_atime: %ld", (long)now, (long)st.st_atime);
	assert(st.st_atime >= now && st.st_atime < now + 3);

	/*
	 * atime specified, but not mtime
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file3");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_atime(ae, 345678, 0);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(ae);
	/* Verify: Current mtime and atime as specified. */
	assertEqualInt(0, stat("file3", &st));
	assertEqualInt(345678, st.st_atime);
	failure("now: %ld st.st_mtime: %ld", (long)now, (long)st.st_mtime);
	assert(st.st_mtime >= now && st.st_mtime < now + 3);

	/*
	 * Neither atime nor mtime specified.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file4");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(ae);
	/* Verify: Current mtime and atime. */
	assertEqualInt(0, stat("file4", &st));
	failure("now: %ld st.st_atime: %ld", (long)now, (long)st.st_atime);
	assert(st.st_atime >= now && st.st_atime < now + 3);
	failure("now: %ld st.st_mtime: %ld", (long)now, (long)st.st_mtime);
	assert(st.st_mtime >= now && st.st_mtime < now + 3);

#if defined(__FreeBSD__)
	/*
	 * High-res mtime and atime on FreeBSD.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file10");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_atime(ae, 1234567, 23456);
	archive_entry_set_mtime(ae, 2345678, 4567);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(ae);
	/* Verify */
	/* FreeBSD can only store usec resolution, hence rounding here. */
	assertEqualInt(0, stat("file10", &st));
	assertEqualInt(1234567, st.st_atime);
	assertEqualInt(23000, st.st_atimespec.tv_nsec);
	assertEqualInt(2345678, st.st_mtime);
	assertEqualInt(4000, st.st_mtimespec.tv_nsec);

	/*
	 * Birthtime, mtime and atime on FreeBSD
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file11");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_atime(ae, 1234567, 23456);
	archive_entry_set_birthtime(ae, 3456789, 12345);
	/* mtime must be later than birthtime! */
	archive_entry_set_mtime(ae, 12345678, 4567);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(ae);
	/* Verify */
	/* FreeBSD can only store usec resolution, hence rounding here. */
	assertEqualInt(0, stat("file11", &st));
	assertEqualInt(1234567, st.st_atime);
	assertEqualInt(23000, st.st_atimespec.tv_nsec);
	assertEqualInt(3456789, st.st_birthtime);
	assertEqualInt(12000, st.st_birthtimespec.tv_nsec);
	assertEqualInt(12345678, st.st_mtime);
	assertEqualInt(4000, st.st_mtimespec.tv_nsec);

	/*
	 * Birthtime only on FreeBSD.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file12");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_birthtime(ae, 3456789, 12345);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(ae);
	/* Verify */
	/* FreeBSD can only store usec resolution, hence rounding here. */
	assertEqualInt(0, stat("file12", &st));
	assertEqualInt(3456789, st.st_birthtime);
	assertEqualInt(12000, st.st_birthtimespec.tv_nsec);
	failure("now: %ld st.st_atime: %ld", (long)now, (long)st.st_atime);
	assert(st.st_atime >= now && st.st_atime < now + 3);
	failure("now: %ld st.st_mtime: %ld", (long)now, (long)st.st_mtime);
	assert(st.st_mtime >= now && st.st_mtime < now + 3);

	/*
	 * mtime only on FreeBSD.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file13");
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_mtime(ae, 4567890, 23456);
	assertEqualInt(ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(ARCHIVE_OK, archive_write_finish_entry(a));
	archive_entry_free(ae);
	/* Verify */
	/* FreeBSD can only store usec resolution, hence rounding here. */
	assertEqualInt(0, stat("file13", &st));
	assertEqualInt(4567890, st.st_birthtime);
	assertEqualInt(23000, st.st_birthtimespec.tv_nsec);
	assertEqualInt(4567890, st.st_mtime);
	assertEqualInt(23000, st.st_mtimespec.tv_nsec);
	failure("now: %ld st.st_atime: %ld", (long)now, (long)st.st_atime);
	assert(st.st_atime >= now && st.st_atime < now + 3);
#else
	skipping("Platform-specific time restore tests");
#endif

	archive_write_finish(a);
}
