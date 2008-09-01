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

static unsigned char testdata[10 * 1024 * 1024];
static unsigned char testdatacopy[10 * 1024 * 1024];
static unsigned char buff[11 * 1024 * 1024];

/* Check correct behavior on large reads. */
DEFINE_TEST(test_read_large)
{
	unsigned int i;
	int tmpfilefd;
	char tmpfilename[] = "/tmp/test-read_large.XXXXXX";
	size_t used;
	struct archive *a;
	struct archive_entry *entry;

	for (i = 0; i < sizeof(testdata); i++)
		testdata[i] = (unsigned char)(rand());

	assert(NULL != (a = archive_write_new()));
	assertA(0 == archive_write_set_format_ustar(a));
	assertA(0 == archive_write_open_memory(a, buff, sizeof(buff), &used));
	assert(NULL != (entry = archive_entry_new()));
	archive_entry_set_size(entry, sizeof(testdata));
	archive_entry_set_mode(entry, S_IFREG | 0777);
	archive_entry_set_pathname(entry, "test");
	assertA(0 == archive_write_header(a, entry));
	archive_entry_free(entry);
	assertA(sizeof(testdata) == archive_write_data(a, testdata, sizeof(testdata)));
#if ARCHIVE_VERSION_NUMBER < 2000000
	archive_write_finish(a);
#else
	assertA(0 == archive_write_finish(a));
#endif

	assert(NULL != (a = archive_read_new()));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_support_compression_all(a));
	assertA(0 == archive_read_open_memory(a, buff, sizeof(buff)));
	assertA(0 == archive_read_next_header(a, &entry));
	assertA(0 == archive_read_data_into_buffer(a, testdatacopy, sizeof(testdatacopy)));
#if ARCHIVE_VERSION_NUMBER < 2000000
	archive_read_finish(a);
#else
	assertA(0 == archive_read_finish(a));
#endif
	assert(0 == memcmp(testdata, testdatacopy, sizeof(testdata)));


	assert(NULL != (a = archive_read_new()));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_support_compression_all(a));
	assertA(0 == archive_read_open_memory(a, buff, sizeof(buff)));
	assertA(0 == archive_read_next_header(a, &entry));
	assert(0 < (tmpfilefd = mkstemp(tmpfilename)));
	assertA(0 == archive_read_data_into_fd(a, tmpfilefd));
	close(tmpfilefd);
#if ARCHIVE_VERSION_NUMBER < 2000000
	archive_read_finish(a);
#else
	assertA(0 == archive_read_finish(a));
#endif
	tmpfilefd = open(tmpfilename, O_RDONLY);
	read(tmpfilefd, testdatacopy, sizeof(testdatacopy));
	close(tmpfilefd);
	assert(0 == memcmp(testdata, testdatacopy, sizeof(testdata)));

	unlink(tmpfilename);
}
