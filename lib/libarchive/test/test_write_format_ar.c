/*-
 * Copyright (c) 2007 Kai Wang
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
__FBSDID("$FreeBSD$");

char buff[4096];
char buff2[64];
static unsigned char strtab[] = "abcdefghijklmn.o/\nggghhhjjjrrrttt.o/\niiijjjdddsssppp.o/\n";

DEFINE_TEST(test_write_format_ar)
{
	struct archive_entry *ae;
	struct archive* a;
	size_t used;

	/*
	 * First we try to create a SVR4/GNU format archive.
	 */
	assert((a = archive_write_new()) != NULL);
	assertA(0 == archive_write_set_format_ar_svr4(a));
	assertA(0 == archive_write_set_compression_gzip(a));
	assertA(0 == archive_write_open_memory(a, buff, sizeof(buff), &used));

	/* write the filename table */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "//");
	archive_entry_set_size(ae, strlen(strtab));
	assertA(0 == archive_write_header(a, ae));
	assertA(strlen(strtab) == (size_t)archive_write_data(a, strtab, strlen(strtab)));
	archive_entry_free(ae);

	/* write entries */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 0);
	assert(1 == archive_entry_mtime(ae));
	archive_entry_set_mode(ae, S_IFREG | 0755);
	assert((S_IFREG | 0755) == archive_entry_mode(ae));
	archive_entry_copy_pathname(ae, "abcdefghijklmn.o");
	archive_entry_set_size(ae, 8);
	assertA(0 == archive_write_header(a, ae));
	assertA(8 == archive_write_data(a, "87654321", 15));
	archive_entry_free(ae);

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "ggghhhjjjrrrttt.o");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, 7);
	assertA(0 == archive_write_header(a, ae));
	assertA(7 == archive_write_data(a, "7777777", 7));
	archive_entry_free(ae);

	/* test full pathname */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "/usr/home/xx/iiijjjdddsssppp.o");
	archive_entry_set_mode(ae, S_IFREG | 0755);
	archive_entry_set_size(ae, 8);
	assertA(0 == archive_write_header(a, ae));
	assertA(8 == archive_write_data(a, "88877766", 8));
	archive_entry_free(ae);

	/* trailing "/" should be rejected */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "/usr/home/xx/iiijjj/");
	archive_entry_set_size(ae, 8);
	assertA(0 != archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Non regular file should be rejected */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "gfgh.o");
	archive_entry_set_mode(ae, S_IFDIR | 0755);
	archive_entry_set_size(ae, 6);
	assertA(0 != archive_write_header(a, ae));
	archive_entry_free(ae);

	archive_write_close(a);
#if ARCHIVE_API_VERSION > 1
	assert(0 == archive_write_finish(a));
#elif
	archive_write_finish(a);
#endif

	/*
	 * Now, read the data back.
	 */
	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_support_compression_all(a));
	assertA(0 == archive_read_open_memory(a, buff, used));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(0, archive_entry_mtime(ae));
	assertEqualString("//", archive_entry_pathname(ae));
	assertEqualInt(strlen(strtab), archive_entry_size(ae));
	assertEqualIntA(a, strlen(strtab), archive_read_data(a, buff2, 100));
	assert(0 == memcmp(buff2, strtab, strlen(strtab)));

	assertA(0 == archive_read_next_header(a, &ae));
	assert(1 == archive_entry_mtime(ae));
	assertEqualString("abcdefghijklmn.o", archive_entry_pathname(ae));
	assert(8 == archive_entry_size(ae));
	assertA(8 == archive_read_data(a, buff2, 10));
	assert(0 == memcmp(buff2, "87654321", 8));

	assert(0 == archive_read_next_header(a, &ae));
	assertEqualString("ggghhhjjjrrrttt.o", archive_entry_pathname(ae));
	assert(7 == archive_entry_size(ae));
	assertA(7 == archive_read_data(a, buff2, 11));
	assert(0 == memcmp(buff2, "7777777", 7));

	assert(0 == archive_read_next_header(a, &ae));
	assertEqualString("iiijjjdddsssppp.o", archive_entry_pathname(ae));
	assert(8 == archive_entry_size(ae));
	assertA(8 == archive_read_data(a, buff2, 17));
	assert(0 == memcmp(buff2, "88877766", 8));

	assert(0 == archive_read_close(a));
#if ARCHIVE_API_VERSION > 1
	assert(0 == archive_read_finish(a));
#else
	archive_read_finish(a);
#endif

	/*
	 * Then, we try to create a BSD format archive.
	 */
	memset(buff, 0, sizeof(buff));
	assert((a = archive_write_new()) != NULL);
	assertA(0 == archive_write_set_format_ar_bsd(a));
	assertA(0 == archive_write_set_compression_bzip2(a));
	assertA(0 == archive_write_open_memory(a, buff, sizeof(buff), &used));

	/* write a entry need long name extension */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "ttttyyyyuuuuiiii.o");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, 5);
	assertA(0 == archive_write_header(a, ae));
	assertA(5 == archive_write_data(a, "12345", 7));
	archive_entry_free(ae);

	/* write a entry with a short name */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, "ttyy.o");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_size(ae, 6);
	assertA(0 == archive_write_header(a, ae));
	assertA(6 == archive_write_data(a, "555555", 7));
	archive_entry_free(ae);
	archive_write_close(a);
#if ARCHIVE_API_VERSION > 1
	assert(0 == archive_write_finish(a));
#elif
	archive_write_finish(a);
#endif

	/* Now, Read the data back */
	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_support_compression_all(a));
	assertA(0 == archive_read_open_memory(a, buff, used));

	assertEqualIntA(a, 0, archive_read_next_header(a, &ae));
	assertEqualString("ttttyyyyuuuuiiii.o", archive_entry_pathname(ae));
	assertEqualInt(5, archive_entry_size(ae));
	assertA(5 == archive_read_data(a, buff2, 10));
	assert(0 == memcmp(buff2, "12345", 5));

	assert(0 == archive_read_next_header(a, &ae));
	assertEqualString("ttyy.o", archive_entry_pathname(ae));
	assert(6 == archive_entry_size(ae));
	assertA(6 == archive_read_data(a, buff2, 10));
	assert(0 == memcmp(buff2, "555555", 6));

	/* Test EOF */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assert(0 == archive_read_close(a));
#if ARCHIVE_API_VERSION > 1
	assert(0 == archive_read_finish(a));
#else
	archive_read_finish(a);
#endif
}
