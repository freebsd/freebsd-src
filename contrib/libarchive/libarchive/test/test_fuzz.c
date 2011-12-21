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
__FBSDID("$FreeBSD: head/lib/libarchive/test/test_fuzz.c 201247 2009-12-30 05:59:21Z kientzle $");

/*
 * This was inspired by an ISO fuzz tester written by Michal Zalewski
 * and posted to the "vulnwatch" mailing list on March 17, 2005:
 *    http://seclists.org/vulnwatch/2005/q1/0088.html
 *
 * This test simply reads each archive image into memory, pokes
 * random values into it and runs it through libarchive.  It tries
 * to damage about 1% of each file and repeats the exercise 100 times
 * with each file.
 *
 * Unlike most other tests, this test does not verify libarchive's
 * responses other than to ensure that libarchive doesn't crash.
 *
 * Due to the deliberately random nature of this test, it may be hard
 * to reproduce failures.  Because this test deliberately attempts to
 * induce crashes, there's little that can be done in the way of
 * post-failure diagnostics.
 */

/* Because this works for any archive, we can just re-use the archives
 * developed for other tests. */
static struct {
	int uncompress; /* If 1, decompress the file before fuzzing. */
	const char *name;
} files[] = {
	{0, "test_fuzz_1.iso.Z"}, /* Exercise compress decompressor. */
	{1, "test_fuzz_1.iso.Z"},
	{0, "test_compat_bzip2_1.tbz"}, /* Exercise bzip2 decompressor. */
	{1, "test_compat_bzip2_1.tbz"},
	{0, "test_compat_gtar_1.tar"},
	{0, "test_compat_gzip_1.tgz"}, /* Exercise gzip decompressor. */
	{0, "test_compat_gzip_2.tgz"}, /* Exercise gzip decompressor. */
	{0, "test_compat_tar_hardlink_1.tar"},
	{0, "test_compat_xz_1.txz"}, /* Exercise xz decompressor. */
	{0, "test_compat_zip_1.zip"},
	{0, "test_read_format_ar.ar"},
	{0, "test_read_format_cpio_bin_be.cpio"},
	{0, "test_read_format_cpio_svr4_gzip_rpm.rpm"}, /* Test RPM unwrapper */
	{0, "test_read_format_gtar_sparse_1_17_posix10_modified.tar"},
	{0, "test_read_format_mtree.mtree"},
	{0, "test_read_format_tar_empty_filename.tar"},
	{0, "test_read_format_zip.zip"},
	{1, NULL}
};

DEFINE_TEST(test_fuzz)
{
	const void *blk;
	size_t blk_size;
	off_t blk_offset;
	int n;

	for (n = 0; files[n].name != NULL; ++n) {
		const size_t buffsize = 30000000;
		const char *filename = files[n].name;
		struct archive_entry *ae;
		struct archive *a;
		char *rawimage, *image;
		size_t size;
		int i;

		extract_reference_file(filename);
		if (files[n].uncompress) {
			int r;
			/* Use format_raw to decompress the data. */
			assert((a = archive_read_new()) != NULL);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_compression_all(a));
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_format_raw(a));
			r = archive_read_open_filename(a, filename, 16384);
			if (r != ARCHIVE_OK) {
				archive_read_finish(a);
				skipping("Cannot uncompress %s", filename);
				continue;
			}
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_next_header(a, &ae));
			rawimage = malloc(buffsize);
			size = archive_read_data(a, rawimage, buffsize);
			assertEqualIntA(a, ARCHIVE_EOF,
			    archive_read_next_header(a, &ae));
			assertEqualInt(ARCHIVE_OK,
			    archive_read_finish(a));
			assert(size > 0);
			failure("Internal buffer is not big enough for "
			    "uncompressed test file: %s", filename);
			if (!assert(size < buffsize)) {
				free(rawimage);
				continue;
			}
		} else {
			rawimage = slurpfile(&size, filename);
			if (!assert(rawimage != NULL))
				continue;
		}
		image = malloc(size);
		assert(image != NULL);
		srand((unsigned)time(NULL));

		for (i = 0; i < 100; ++i) {
			FILE *f;
			int j, numbytes;

			/* Fuzz < 1% of the bytes in the archive. */
			memcpy(image, rawimage, size);
			numbytes = (int)(rand() % (size / 100));
			for (j = 0; j < numbytes; ++j)
				image[rand() % size] = (char)rand();

			/* Save the messed-up image to a file.
			 * If we crash, that file will be useful. */
			f = fopen("after.test.failure.send.this.file."
			    "to.libarchive.maintainers.with.system.details", "wb");
			fwrite(image, 1, (size_t)size, f);
			fclose(f);

			assert((a = archive_read_new()) != NULL);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_compression_all(a));
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_format_all(a));

			if (0 == archive_read_open_memory(a, image, size)) {
				while(0 == archive_read_next_header(a, &ae)) {
					while (0 == archive_read_data_block(a,
						&blk, &blk_size, &blk_offset))
						continue;
				}
				archive_read_close(a);
			}
			archive_read_finish(a);
		}
		free(image);
		free(rawimage);
	}
}


