/*-
 * Copyright (c) 2017 Sean Purcell
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

DEFINE_TEST(test_write_filter_zstd)
{
	struct archive_entry *ae;
	struct archive *a;
	char *buff, *data;
	size_t buffsize, datasize;
	char path[16];
	size_t used1, used2, used3;
	int i, r;

	buffsize = 2000000;
	assert(NULL != (buff = malloc(buffsize)));
	if (buff == NULL)
		return;

	datasize = 10000;
	assert(NULL != (data = malloc(datasize)));
	if (data == NULL) {
		free(buff);
		return;
	}
	memset(data, 0, datasize);

	/*
	 * Write a 100 files and read them all back.
	 */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	r = archive_write_add_filter_zstd(a);
	if (r != ARCHIVE_OK) {
		skipping("zstd writing not supported on this platform");
		assertEqualInt(ARCHIVE_OK, archive_write_free(a));
		free(buff);
		free(data);
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_bytes_per_block(a, 10));
	assertEqualInt(ARCHIVE_FILTER_ZSTD, archive_filter_code(a, 0));
	assertEqualString("zstd", archive_filter_name(a, 0));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, buffsize, &used1));
	assertEqualInt(ARCHIVE_FILTER_ZSTD, archive_filter_code(a, 0));
	assertEqualString("zstd", archive_filter_name(a, 0));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, datasize);
	for (i = 0; i < 100; i++) {
		snprintf(path, sizeof(path), "file%03d", i);
		archive_entry_copy_pathname(ae, path);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		assertA(datasize
		    == (size_t)archive_write_data(a, data, datasize));
	}
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	r = archive_read_support_filter_zstd(a);
	if (r == ARCHIVE_WARN) {
		skipping("Can't verify zstd writing by reading back;"
		    " zstd reading not fully supported on this platform");
	} else {
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_filter_all(a));
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_open_memory(a, buff, used1));
		for (i = 0; i < 100; i++) {
			snprintf(path, sizeof(path), "file%03d", i);
			if (!assertEqualInt(ARCHIVE_OK,
				archive_read_next_header(a, &ae)))
				break;
			assertEqualString(path, archive_entry_pathname(ae));
			assertEqualInt((int)datasize, archive_entry_size(ae));
		}
		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * Repeat the cycle again, this time setting some compression
	 * options.
	 */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_bytes_per_block(a, 10));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_zstd(a));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "nonexistent-option", "0"));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "compression-level", "abc"));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "compression-level", "25")); /* too big */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "compression-level", "9"));
	/* Following is disabled as it will fail on library versions < 1.3.4 */
	/* assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "compression-level", "-1")); */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "compression-level", "7"));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "threads", "-1")); /* negative */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "threads", "4"));
#if HAVE_ZSTD_H && HAVE_ZSTD_compressStream
	/* frame-per-file: boolean */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "frame-per-file", ""));
	/* min-frame-in: >= 0 */
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "min-frame-out", ""));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "min-frame-out", "-1"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-out", "0"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-out", "1048576"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-out", "1k"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-out", "1kB"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-out", "1M"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-out", "1MB"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-out", "1G"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-out", "1GB"));
	/* min-frame-out: >= 0 */
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "min-frame-in", ""));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "min-frame-in", "-1"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-in", "0"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-in", "1048576"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-in", "1k"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-in", "1kB"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-in", "1M"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-in", "1MB"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-in", "1G"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "min-frame-in", "1GB"));
	/* max-frame-in: >= 1024 */
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", ""));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", "-1"));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", "0"));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", "1023"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", "1024"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", "1048576"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", "1k"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", "1kB"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", "1M"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", "1MB"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", "1G"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-in", "1GB"));
	/* max-frame-out: >= 1024 */
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", ""));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", "-1"));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", "0"));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", "1023"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", "1024"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", "1048576"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", "1k"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", "1kB"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", "1M"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", "1MB"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", "1G"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "max-frame-out", "1GB"));
#endif
#if ZSTD_VERSION_NUMBER >= MINVER_LONG
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "long", "23"));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_filter_option(a, NULL, "long", "-1")); /* negative */
#endif
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, buffsize, &used2));
	for (i = 0; i < 100; i++) {
		snprintf(path, sizeof(path), "file%03d", i);
		assert((ae = archive_entry_new()) != NULL);
		archive_entry_copy_pathname(ae, path);
		archive_entry_set_size(ae, datasize);
		archive_entry_set_filetype(ae, AE_IFREG);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		assertA(datasize == (size_t)archive_write_data(a, data, datasize));
		archive_entry_free(ae);
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	r = archive_read_support_filter_zstd(a);
	if (r == ARCHIVE_WARN) {
		skipping("zstd reading not fully supported on this platform");
	} else {
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_filter_all(a));
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_open_memory(a, buff, used2));
		for (i = 0; i < 100; i++) {
			snprintf(path, sizeof(path), "file%03d", i);
			failure("Trying to read %s", path);
			if (!assertEqualIntA(a, ARCHIVE_OK,
				archive_read_next_header(a, &ae)))
				break;
			assertEqualString(path, archive_entry_pathname(ae));
			assertEqualInt((int)datasize, archive_entry_size(ae));
		}
		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * One more time at level 1
	 */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_bytes_per_block(a, 10));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_zstd(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_filter_option(a, NULL, "compression-level", "1"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, buffsize, &used3));
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, datasize);
	for (i = 0; i < 100; i++) {
		snprintf(path, sizeof(path), "file%03d", i);
		archive_entry_copy_pathname(ae, path);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		assertA(datasize == (size_t)archive_write_data(a, data, datasize));
	}
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	r = archive_read_support_filter_zstd(a);
	if (r == ARCHIVE_WARN) {
		skipping("zstd reading not fully supported on this platform");
	} else {
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_filter_all(a));
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_open_memory(a, buff, used3));
		for (i = 0; i < 100; i++) {
			snprintf(path, sizeof(path), "file%03d", i);
			failure("Trying to read %s", path);
			if (!assertEqualIntA(a, ARCHIVE_OK,
				archive_read_next_header(a, &ae)))
				break;
			assertEqualString(path, archive_entry_pathname(ae));
			assertEqualInt((int)datasize, archive_entry_size(ae));
		}
		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/*
	 * Check output sizes for various compression levels, expectation
	 * is that archive size for level=7 < default < level=1
	 */
	failure("compression-level=7 wrote %d bytes, default wrote %d bytes",
	    (int)used2, (int)used1);
	assert(used2 < used1);
	failure("compression-level=1 wrote %d bytes, default wrote %d bytes",
	    (int)used3, (int)used1);
	assert(used1 < used3);

	/*
	 * Test various premature shutdown scenarios to make sure we
	 * don't crash or leak memory.
	 */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_zstd(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_zstd(a));
	assertEqualInt(ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_zstd(a));
	assertEqualInt(ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_zstd(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, buffsize, &used2));
	assertEqualInt(ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/*
	 * Clean up.
	 */
	free(data);
	free(buff);
}
