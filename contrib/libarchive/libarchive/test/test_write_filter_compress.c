/*-
 * Copyright (c) 2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 * A basic exercise of compress reading and writing.
 *
 * TODO: Add a reference file and make sure we can decompress that.
 */

DEFINE_TEST(test_write_filter_compress)
{
	struct archive_entry *ae;
	struct archive* a;
	char *buff, *data;
	size_t buffsize, datasize;
	char path[16];
	size_t used;
	int i;

	buffsize = 1000000;
	assert(NULL != (buff = malloc(buffsize)));

	datasize = 10000;
	assert(NULL != (data = malloc(datasize)));
	memset(data, 0, datasize);

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_add_filter_compress(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	for (i = 0; i < 100; i++) {
		snprintf(path, sizeof(path), "file%03d", i);
		assert((ae = archive_entry_new()) != NULL);
		archive_entry_copy_pathname(ae, path);
		archive_entry_set_size(ae, datasize);
		archive_entry_set_filetype(ae, AE_IFREG);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		assertEqualInt(datasize,
		    archive_write_data(a, data, datasize));
		archive_entry_free(ae);
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/*
	 * Now, read the data back.
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));


	for (i = 0; i < 100; i++) {
		snprintf(path, sizeof(path), "file%03d", i);
		if (!assertEqualInt(0, archive_read_next_header(a, &ae)))
			break;
		assertEqualString(path, archive_entry_pathname(ae));
		assertEqualInt((int)datasize, archive_entry_size(ae));
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(data);
	free(buff);
}

/*
 * A write callback that fails after it has accepted a threshold number
 * of bytes, modelling a full disk or a broken pipe in the middle of the
 * compressed output.
 */
struct failing_write_state {
	size_t fail_after;
	size_t written_so_far;
};

static ssize_t
failing_write(struct archive *a, void *client_data, const void *buff,
    size_t length)
{
	struct failing_write_state *state = client_data;

	(void)a;
	(void)buff;
	state->written_so_far += length;
	if (state->written_so_far > state->fail_after)
		return (-1);
	return ((ssize_t)length);
}

/*
 * Regression test: if the client write callback fails while the compress
 * (.Z) filter is flushing a full output buffer, the filter must not keep
 * writing past the end of that buffer.  Before the fix, output_byte()
 * left the buffer offset at buffer_size on a failed flush and
 * output_code() issued further output_byte() calls without checking the
 * return value, overflowing the heap buffer by one byte.  The body is
 * incompressible so the filter emits output steadily and is guaranteed
 * to hit a flush while the sink is failing.
 */
DEFINE_TEST(test_write_filter_compress_failed_write)
{
	struct failing_write_state state;
	struct archive *a;
	struct archive_entry *ae;
	size_t datasize = 2 * 1024 * 1024;
	unsigned char *data;
	size_t i;
	uint32_t seed = 0x12345;

	assert(NULL != (data = malloc(datasize)));
	for (i = 0; i < datasize; i++) {
		seed = seed * 1103515245u + 12345u;
		data[i] = (unsigned char)(seed >> 16);
	}

	state.written_so_far = 0;
	state.fail_after = 70000;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_compress(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open(a, &state, NULL, failing_write, NULL));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "file");
	archive_entry_set_size(ae, datasize);
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, 0644);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/*
	 * The write must fail cleanly (no out-of-bounds access).  We do
	 * not care exactly which call reports it, only that nothing writes
	 * past the compress buffer; an ASan/UBSan build would abort here
	 * before the fix.
	 */
	archive_write_data(a, data, datasize);
	assertEqualIntA(a, ARCHIVE_FATAL, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	free(data);
}
