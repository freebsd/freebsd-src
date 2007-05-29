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

static void create(struct archive_entry *ae, const char *msg)
{
	struct archive *ad;
	struct stat st;

	/* Write the entry to disk. */
	assert((ad = archive_write_disk_new()) != NULL);
	failure("%s", msg);
	assertEqualIntA(ad, 0, archive_write_header(ad, ae));
	assertEqualIntA(ad, 0, archive_write_finish_entry(ad));
#if ARCHIVE_API_VERSION > 1
	assertEqualInt(0, archive_write_finish(ad));
#else
	archive_write_finish(ad);
#endif
	/* Test the entries on disk. */
	assert(0 == stat(archive_entry_pathname(ae), &st));
	failure("st.st_mode=%o archive_entry_mode(ae)=%o",
	    st.st_mode, archive_entry_mode(ae));
	assert(st.st_mode == (archive_entry_mode(ae) & ~UMASK));
}

DEFINE_TEST(test_write_disk)
{
	struct archive_entry *ae;

	/* Force the umask to something predictable. */
	umask(UMASK);

	/* A regular file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	create(ae, "Test creating a regular file");
	archive_entry_free(ae);

	/* A regular file over an existing file */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0724);
	create(ae, "Test creating a file over an existing file.");
	archive_entry_free(ae);

	/* A directory. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "dir");
	archive_entry_set_mode(ae, S_IFDIR | 0555);
	create(ae, "Test creating a regular dir.");
	archive_entry_free(ae);

	/* A directory over an existing file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFDIR | 0742);
	create(ae, "Test creating a dir over an existing file.");
	archive_entry_free(ae);

	/* A file over an existing dir. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0744);
	create(ae, "Test creating a file over an existing dir.");
	archive_entry_free(ae);
}
