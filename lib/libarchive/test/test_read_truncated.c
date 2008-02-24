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
__FBSDID("$FreeBSD: src/lib/libarchive/test/test_read_truncated.c,v 1.3 2007/05/29 01:00:21 kientzle Exp $");

char buff[1000000];
char buff2[100000];

DEFINE_TEST(test_read_truncated)
{
	struct archive_entry *ae;
	struct archive *a;
	unsigned int i;
	size_t used;

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertA(0 == archive_write_set_format_ustar(a));
	assertA(0 == archive_write_set_compression_none(a));
	assertA(0 == archive_write_open_memory(a, buff, sizeof(buff), &used));

	/*
	 * Write a file to it.
	 */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	for (i = 0; i < sizeof(buff2); i++)
		buff2[i] = (unsigned char)rand();
	archive_entry_set_size(ae, sizeof(buff2));
	assertA(0 == archive_write_header(a, ae));
	archive_entry_free(ae);
	assertA(sizeof(buff2) == archive_write_data(a, buff2, sizeof(buff2)));

	/* Close out the archive. */
	assertA(0 == archive_write_close(a));
#if ARCHIVE_API_VERSION > 1
	assertA(0 == archive_write_finish(a));
#else
	archive_write_finish(a);
#endif

	/* Now, read back a truncated version of the archive and
	 * verify that we get an appropriate error. */
	for (i = 1; i < used + 100; i += 100) {
		assert((a = archive_read_new()) != NULL);
		assertA(0 == archive_read_support_format_all(a));
		assertA(0 == archive_read_support_compression_all(a));
		assertA(0 == archive_read_open_memory(a, buff, i));

		if (i < 512) {
			assertA(ARCHIVE_FATAL == archive_read_next_header(a, &ae));
			goto wrap_up;
		} else {
			assertA(0 == archive_read_next_header(a, &ae));
		}

		if (i < 512 + sizeof(buff2)) {
			assertA(ARCHIVE_FATAL == archive_read_data(a, buff2, sizeof(buff2)));
			goto wrap_up;
		} else {
			assertA(sizeof(buff2) == archive_read_data(a, buff2, sizeof(buff2)));
		}

		/* Verify the end of the archive. */
		/* Archive must be long enough to capture a 512-byte
		 * block of zeroes after the entry.  (POSIX requires a
		 * second block of zeros to be written but libarchive
		 * does not return an error if it can't consume
		 * it.) */
		if (i < 512 + 512*((sizeof(buff2) + 511)/512) + 512) {
			assertA(ARCHIVE_FATAL == archive_read_next_header(a, &ae));
		} else {
			assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
		}
	wrap_up:
		assert(0 == archive_read_close(a));
#if ARCHIVE_API_VERSION > 1
		assert(0 == archive_read_finish(a));
#else
		archive_read_finish(a);
#endif
	}



	/* Same as above, except skip the body instead of reading it. */
	for (i = 1; i < used + 100; i += 100) {
		assert((a = archive_read_new()) != NULL);
		assertA(0 == archive_read_support_format_all(a));
		assertA(0 == archive_read_support_compression_all(a));
		assertA(0 == archive_read_open_memory(a, buff, i));

		if (i < 512) {
			assertA(ARCHIVE_FATAL == archive_read_next_header(a, &ae));
			goto wrap_up2;
		} else {
			assertA(0 == archive_read_next_header(a, &ae));
		}

		if (i < 512 + 512*((sizeof(buff2)+511)/512)) {
			assertA(ARCHIVE_FATAL == archive_read_data_skip(a));
			goto wrap_up2;
		} else {
			assertA(ARCHIVE_OK == archive_read_data_skip(a));
		}

		/* Verify the end of the archive. */
		/* Archive must be long enough to capture a 512-byte
		 * block of zeroes after the entry.  (POSIX requires a
		 * second block of zeros to be written but libarchive
		 * does not return an error if it can't consume
		 * it.) */
		if (i < 512 + 512*((sizeof(buff2) + 511)/512) + 512) {
			assertA(ARCHIVE_FATAL == archive_read_next_header(a, &ae));
		} else {
			assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
		}
	wrap_up2:
		assert(0 == archive_read_close(a));
#if ARCHIVE_API_VERSION > 1
		assert(0 == archive_read_finish(a));
#else
		archive_read_finish(a);
#endif
	}
}
