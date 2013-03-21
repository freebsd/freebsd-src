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
struct files {
	int uncompress; /* If 1, decompress the file before fuzzing. */
	const char **names;
};

static void
test_fuzz(const struct files *filesets)
{
	const void *blk;
	size_t blk_size;
	int64_t blk_offset;
	int n;

	for (n = 0; filesets[n].names != NULL; ++n) {
		const size_t buffsize = 30000000;
		struct archive_entry *ae;
		struct archive *a;
		char *rawimage = NULL, *image = NULL, *tmp = NULL;
		size_t size = 0, oldsize = 0;
		int i, q;

		extract_reference_files(filesets[n].names);
		if (filesets[n].uncompress) {
			int r;
			/* Use format_raw to decompress the data. */
			assert((a = archive_read_new()) != NULL);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_filter_all(a));
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_format_raw(a));
			r = archive_read_open_filenames(a, filesets[n].names, 16384);
			if (r != ARCHIVE_OK) {
				archive_read_free(a);
				if (filesets[n].names[0] == NULL || filesets[n].names[1] == NULL) {
					skipping("Cannot uncompress fileset");
				} else {
					skipping("Cannot uncompress %s", filesets[n].names[0]);
				}
				continue;
			}
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_next_header(a, &ae));
			rawimage = malloc(buffsize);
			size = archive_read_data(a, rawimage, buffsize);
			assertEqualIntA(a, ARCHIVE_EOF,
			    archive_read_next_header(a, &ae));
			assertEqualInt(ARCHIVE_OK,
			    archive_read_free(a));
			assert(size > 0);
			if (filesets[n].names[0] == NULL || filesets[n].names[1] == NULL) {
				failure("Internal buffer is not big enough for "
					"uncompressed test files");
			} else {
				failure("Internal buffer is not big enough for "
					"uncompressed test file: %s", filesets[n].names[0]);
			}
			if (!assert(size < buffsize)) {
				free(rawimage);
				continue;
			}
		} else {
			for (i = 0; filesets[n].names[i] != NULL; ++i)
			{
				tmp = slurpfile(&size, filesets[n].names[i]);
				rawimage = (char *)realloc(rawimage, oldsize + size);
				memcpy(rawimage + oldsize, tmp, size);
				oldsize += size;
				size = oldsize;
				free(tmp);
				if (!assert(rawimage != NULL))
					continue;
			}
		}
		if (size == 0)
			continue;
		image = malloc(size);
		assert(image != NULL);
		if (image == NULL)
			return;
		srand((unsigned)time(NULL));

		for (i = 0; i < 100; ++i) {
			FILE *f;
			int j, numbytes, trycnt;

			/* Fuzz < 1% of the bytes in the archive. */
			memcpy(image, rawimage, size);
			q = (int)size / 100;
			if (!q) q = 1;
			numbytes = (int)(rand() % q);
			for (j = 0; j < numbytes; ++j)
				image[rand() % size] = (char)rand();

			/* Save the messed-up image to a file.
			 * If we crash, that file will be useful. */
			for (trycnt = 0; trycnt < 3; trycnt++) {
				f = fopen("after.test.failure.send.this.file."
				    "to.libarchive.maintainers.with.system.details", "wb");
				if (f != NULL)
					break;
#if defined(_WIN32) && !defined(__CYGWIN__)
				/*
				 * Sometimes previous close operation does not completely
				 * end at this time. So we should take a wait while
				 * the operation running.
				 */
				Sleep(100);
#endif
			}
			assertEqualInt((size_t)size, fwrite(image, 1, (size_t)size, f));
			fclose(f);

			assert((a = archive_read_new()) != NULL);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_filter_all(a));
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
			archive_read_free(a);
		}
		free(image);
		free(rawimage);
	}
}

DEFINE_TEST(test_fuzz_ar)
{
	static const char *fileset1[] = {
		"test_read_format_ar.ar",
		NULL
	};
	static const struct files filesets[] = {
		{0, fileset1},
		{1, NULL}
	};
	test_fuzz(filesets);
}

DEFINE_TEST(test_fuzz_cab)
{
	static const char *fileset1[] = {
		"test_fuzz.cab",
		NULL
	};
	static const struct files filesets[] = {
		{0, fileset1},
		{1, NULL}
	};
	test_fuzz(filesets);
}

DEFINE_TEST(test_fuzz_cpio)
{
	static const char *fileset1[] = {
		"test_read_format_cpio_bin_be.cpio",
		NULL
	};
	static const char *fileset2[] = {
		/* Test RPM unwrapper */
		"test_read_format_cpio_svr4_gzip_rpm.rpm",
		NULL
	};
	static const struct files filesets[] = {
		{0, fileset1},
		{0, fileset2},
		{1, NULL}
	};
	test_fuzz(filesets);
}

DEFINE_TEST(test_fuzz_iso9660)
{
	static const char *fileset1[] = {
		"test_fuzz_1.iso.Z",
		NULL
	};
	static const struct files filesets[] = {
		{0, fileset1}, /* Exercise compress decompressor. */
		{1, fileset1},
		{1, NULL}
	};
	test_fuzz(filesets);
}

DEFINE_TEST(test_fuzz_lzh)
{
	static const char *fileset1[] = {
		"test_fuzz.lzh",
		NULL
	};
	static const struct files filesets[] = {
		{0, fileset1},
		{1, NULL}
	};
	test_fuzz(filesets);
}

DEFINE_TEST(test_fuzz_mtree)
{
	static const char *fileset1[] = {
		"test_read_format_mtree.mtree",
		NULL
	};
	static const struct files filesets[] = {
		{0, fileset1},
		{1, NULL}
	};
	test_fuzz(filesets);
}

DEFINE_TEST(test_fuzz_rar)
{
	static const char *fileset1[] = {
		/* Uncompressed RAR test */
		"test_read_format_rar.rar",
		NULL
	};
	static const char *fileset2[] = {
		/* RAR file with binary data */
		"test_read_format_rar_binary_data.rar",
		NULL
	};
	static const char *fileset3[] = {
		/* Best Compressed RAR test */
		"test_read_format_rar_compress_best.rar",
		NULL
	};
	static const char *fileset4[] = {
		/* Normal Compressed RAR test */
		"test_read_format_rar_compress_normal.rar",
		NULL
	};
	static const char *fileset5[] = {
		/* Normal Compressed Multi LZSS blocks RAR test */
		"test_read_format_rar_multi_lzss_blocks.rar",
		NULL
	};
	static const char *fileset6[] = {
		/* RAR with no EOF header */
		"test_read_format_rar_noeof.rar",
		NULL
	};
	static const char *fileset7[] = {
		/* Best Compressed RAR file with both PPMd and LZSS blocks */
		"test_read_format_rar_ppmd_lzss_conversion.rar",
		NULL
	};
	static const char *fileset8[] = {
		/* RAR with subblocks */
		"test_read_format_rar_subblock.rar",
		NULL
	};
	static const char *fileset9[] = {
		/* RAR with Unicode filenames */
		"test_read_format_rar_unicode.rar",
		NULL
	};
	static const char *fileset10[] = {
		"test_read_format_rar_multivolume.part0001.rar",
		"test_read_format_rar_multivolume.part0002.rar",
		"test_read_format_rar_multivolume.part0003.rar",
		"test_read_format_rar_multivolume.part0004.rar",
		NULL
	};
	static const struct files filesets[] = {
		{0, fileset1},
		{0, fileset2},
		{0, fileset3},
		{0, fileset4},
		{0, fileset5},
		{0, fileset6},
		{0, fileset7},
		{0, fileset8},
		{0, fileset9},
		{0, fileset10},
		{1, NULL}
	};
	test_fuzz(filesets);
}

DEFINE_TEST(test_fuzz_tar)
{
	static const char *fileset1[] = {
		"test_compat_bzip2_1.tbz",
		NULL
	};
	static const char *fileset2[] = {
		"test_compat_gtar_1.tar",
		NULL
	};
	static const char *fileset3[] = {
		"test_compat_gzip_1.tgz",
		NULL
	};
	static const char *fileset4[] = {
		"test_compat_gzip_2.tgz",
		NULL
	};
	static const char *fileset5[] = {
		"test_compat_tar_hardlink_1.tar",
		NULL
	};
	static const char *fileset6[] = {
		"test_compat_xz_1.txz",
		NULL
	};
	static const char *fileset7[] = {
		"test_read_format_gtar_sparse_1_17_posix10_modified.tar",
		NULL
	};
	static const char *fileset8[] = {
		"test_read_format_tar_empty_filename.tar",
		NULL
	};
	static const char *fileset9[] = {
		"test_compat_lzop_1.tar.lzo",
		NULL
	};
	static const struct files filesets[] = {
		{0, fileset1}, /* Exercise bzip2 decompressor. */
		{1, fileset1},
		{0, fileset2},
		{0, fileset3}, /* Exercise gzip decompressor. */
		{0, fileset4}, /* Exercise gzip decompressor. */
		{0, fileset5},
		{0, fileset6}, /* Exercise xz decompressor. */
		{0, fileset7},
		{0, fileset8},
		{0, fileset9}, /* Exercise lzo decompressor. */
		{1, NULL}
	};
	test_fuzz(filesets);
}

DEFINE_TEST(test_fuzz_zip)
{
	static const char *fileset1[] = {
		"test_compat_zip_1.zip",
		NULL
	};
	static const char *fileset2[] = {
		"test_read_format_zip.zip",
		NULL
	};
	static const struct files filesets[] = {
		{0, fileset1},
		{0, fileset2},
		{1, NULL}
	};
	test_fuzz(filesets);
}

