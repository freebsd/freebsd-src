/*-
 * Copyright (c) 2026 GeorgH93
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

static const char *password = "test_password_zipx";

/* 512 bytes of test data — large enough to exercise decompression. */
static const char file_data[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz\n"
    "The quick brown fox jumps over the lazy dog. Pack my box with five\n"
    "dozen liquor jugs. How vexingly quick daft zebras jump! Bright\n"
    "vixens jump; dozy fowl quack. Jackdaws love my big sphinx of\n"
    "quartz. The five boxing wizards jump quickly. Amazingly few\n"
    "discotheques provide jukeboxes. Crazy Frederick bought many very\n"
    "exquisite opal jewels. We promptly judged antique ivory buckles\n"
    "for the next prize. Sixty zippers were quickly picked from the bag.\n";

static void
validate_entry_read(struct archive *a, const char* msg)
{
	struct archive_entry *ae;
	size_t total_read;
	ssize_t r;
	char readbuf[1024];

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("encrypted_file.txt", archive_entry_pathname(ae));
	assertEqualInt((int)(sizeof(file_data) - 1),
				   archive_entry_size(ae));
	assertEqualInt(1, archive_entry_is_data_encrypted(ae));

	total_read = 0;
	for (;;) {
		r = archive_read_data(a, readbuf, sizeof(readbuf));
		if (r <= 0) {
			if (r < 0) {
				failure(msg, (int) r, archive_error_string(a));
				assertEqualInt(r > 0, 1);
			}
			break;
		}
		assertEqualMem(readbuf, file_data + total_read, (size_t)r);
		total_read += (size_t)r;
	}
	assertEqualInt((int)total_read, (int)(sizeof(file_data) - 1));
}

static void
test_encrypted_zipx(const char *compression_name)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t used;
	size_t buffsize = 1000000;
	char *buff;
	char readbuf[1024];

	buff = malloc(buffsize);
	assert(buff != NULL);

	/* ---- Write encrypted zipx archive ---- */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));

	if (ARCHIVE_OK != archive_write_set_options(a,
			"zip:encryption=aes256")) {
		skipping("This system does not have cryptographic library");
		archive_write_free(a);
		free(buff);
		return;
	}

	if (ARCHIVE_OK != archive_write_set_options(a, compression_name)) {
		skipping("%s is not supported on this platform", compression_name);
		archive_write_free(a);
		free(buff);
		return;
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_passphrase(a, password));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "zip:experimental"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, buffsize, &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 0);
	archive_entry_copy_pathname(ae, "encrypted_file.txt");
	archive_entry_set_mode(ae, AE_IFREG | 0644);
	archive_entry_set_size(ae, sizeof(file_data) - 1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualInt(sizeof(file_data) - 1, archive_write_data(a, file_data, sizeof(file_data) - 1));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* ---- Read back without password — should fail ---- */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("encrypted_file.txt", archive_entry_pathname(ae));
	assertEqualInt(1, archive_entry_is_data_encrypted(ae));
	assertEqualInt(0, archive_entry_is_metadata_encrypted(ae));
	assertEqualInt(ARCHIVE_FAILED, archive_read_data(a, readbuf, sizeof(readbuf)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* ---- Read back with correct password ---- */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_add_passphrase(a, password));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));

	validate_entry_read(a, "archive_read_data returned %d: %s");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* ---- Seeking reader with correct password ---- */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_add_passphrase(a, password));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, buff, used, 7));

	validate_entry_read(a, "seek: archive_read_data returned %d: %s");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	free(buff);
}

DEFINE_TEST(test_read_format_zip_deflate_encrypted)
{
	test_encrypted_zipx("zip:compression=deflate");
}

DEFINE_TEST(test_read_format_zip_zipx_bzip2_encrypted)
{
	test_encrypted_zipx("zip:compression=bzip2");
}

DEFINE_TEST(test_read_format_zip_zipx_lzma_encrypted)
{
	test_encrypted_zipx("zip:compression=lzma");
}

DEFINE_TEST(test_read_format_zip_zipx_xz_encrypted)
{
	test_encrypted_zipx("zip:compression=xz");
}

DEFINE_TEST(test_read_format_zip_zipx_zstd_encrypted)
{
	test_encrypted_zipx("zip:compression=zstd");
}
