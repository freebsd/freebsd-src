/*-
 * Copyright (c) 2018 Grzegorz Antoniak
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

/* Some tests will want to calculate some CRC32's, and this header can
 * help. */
#define __LIBARCHIVE_BUILD
#include <archive_crc32.h>
#include <archive_endian.h>

#define PROLOGUE(reffile) \
	struct archive_entry *ae; \
	struct archive *a; \
	\
	(void) a;  /* Make the compiler happy if we won't use this variables */ \
	(void) ae; /* in the test cases. */ \
	\
	extract_reference_file(reffile); \
	assert((a = archive_read_new()) != NULL); \
	assertA(0 == archive_read_support_filter_all(a)); \
	assertA(0 == archive_read_support_format_all(a)); \
	assertA(0 == archive_read_open_filename(a, reffile, 10240))

#define PROLOGUE_MULTI(reffile) \
	struct archive_entry *ae; \
	struct archive *a; \
	\
	(void) a; \
	(void) ae; \
	\
	extract_reference_files(reffile); \
	assert((a = archive_read_new()) != NULL); \
	assertA(0 == archive_read_support_filter_all(a)); \
	assertA(0 == archive_read_support_format_all(a)); \
	assertA(0 == archive_read_open_filenames(a, reffile, 10240))


#define EPILOGUE() \
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a)); \
	assertEqualInt(ARCHIVE_OK, archive_read_free(a))

static
int verify_data(const uint8_t* data_ptr, int magic, int size) {
	int i = 0;

	/* This is how the test data inside test files was generated;
	 * we are re-generating it here and we check if our re-generated
	 * test data is the same as in the test file. If this test is
	 * failing it's either because there's a bug in the test case,
	 * or the unpacked data is corrupted. */

	for(i = 0; i < size / 4; ++i) {
		const int k = i + 1;
		const signed int* lptr = (const signed int*) &data_ptr[i * 4];
		signed int val = k * k - 3 * k + (1 + magic);

		if(val < 0)
			val = 0;

		/* *lptr is a value inside unpacked test file, val is the
		 * value that should be in the unpacked test file. */

		if(archive_le32dec(lptr) != (uint32_t) val)
			return 0;
	}

	return 1;
}

static
int extract_one(struct archive* a, struct archive_entry* ae, uint32_t crc) {
	la_ssize_t fsize, bytes_read;
	uint8_t* buf;
	int ret = 1;
	uint32_t computed_crc;

	fsize = (la_ssize_t) archive_entry_size(ae);
	buf = malloc(fsize);
	if(buf == NULL)
		return 1;

	bytes_read = archive_read_data(a, buf, fsize);
	if(bytes_read != fsize) {
		assertEqualInt(bytes_read, fsize);
		goto fn_exit;
	}

	computed_crc = crc32(0, buf, fsize);
	assertEqualInt(computed_crc, crc);
	ret = 0;

fn_exit:
	free(buf);
	return ret;
}

DEFINE_TEST(test_read_format_rar5_set_format)
{
	struct archive *a;
	struct archive_entry *ae;
	const char reffile[] = "test_read_format_rar5_stored.rar";

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_filter_all(a));
	assertA(0 == archive_read_set_format(a, ARCHIVE_FORMAT_RAR_V5));
	assertA(0 == archive_read_open_filename(a, reffile, 10240));
	assertA(0 == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_stored)
{
	const char helloworld_txt[] = "hello libarchive test suite!\n";
	la_ssize_t file_size = sizeof(helloworld_txt) - 1;
	char buff[64];

	PROLOGUE("test_read_format_rar5_stored.rar");

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("helloworld.txt", archive_entry_pathname(ae));
	assertA((int) archive_entry_mtime(ae) > 0);
	assertA((int) archive_entry_ctime(ae) == 0);
	assertA((int) archive_entry_atime(ae) == 0);
	assertEqualInt(file_size, archive_entry_size(ae));
	assertEqualInt(33188, archive_entry_mode(ae));
	assertA(file_size == archive_read_data(a, buff, file_size));
	assertEqualMem(buff, helloworld_txt, file_size);
	assertEqualInt(archive_entry_is_encrypted(ae), 0);

	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_compressed)
{
	const int DATA_SIZE = 1200;
	uint8_t buff[1200];

	PROLOGUE("test_read_format_rar5_compressed.rar");

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA((int) archive_entry_mtime(ae) > 0);
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	assertA(1 == verify_data(buff, 0, DATA_SIZE));

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_multiple_files)
{
	const int DATA_SIZE = 4096;
	uint8_t buff[4096];

	PROLOGUE("test_read_format_rar5_multiple_files.rar");

	/* There should be 4 files inside this test file. Check for their
	 * existence, and also check the contents of those test files. */

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
	assertA(1 == verify_data(buff, 1, DATA_SIZE));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
	assertA(1 == verify_data(buff, 2, DATA_SIZE));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
	assertA(1 == verify_data(buff, 3, DATA_SIZE));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
	assertA(1 == verify_data(buff, 4, DATA_SIZE));

	/* There should be no more files in this archive. */

	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

/* This test is really the same as the test above, but it deals with a solid
 * archive instead of a regular archive. The test solid archive contains the
 * same set of files as regular test archive, but it's size is 2x smaller,
 * because solid archives reuse the window buffer from previous compressed
 * files, so it's able to compress lots of small files more effectively. */

DEFINE_TEST(test_read_format_rar5_multiple_files_solid)
{
	const int DATA_SIZE = 4096;
	uint8_t buff[4096];

	PROLOGUE("test_read_format_rar5_multiple_files_solid.rar");

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
	assertA(1 == verify_data(buff, 1, DATA_SIZE));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
	assertA(1 == verify_data(buff, 2, DATA_SIZE));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
	assertA(1 == verify_data(buff, 3, DATA_SIZE));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));
	assertA(1 == verify_data(buff, 4, DATA_SIZE));

	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_multiarchive_skip_all)
{
	const char* reffiles[] = {
		"test_read_format_rar5_multiarchive.part01.rar",
		"test_read_format_rar5_multiarchive.part02.rar",
		"test_read_format_rar5_multiarchive.part03.rar",
		"test_read_format_rar5_multiarchive.part04.rar",
		"test_read_format_rar5_multiarchive.part05.rar",
		"test_read_format_rar5_multiarchive.part06.rar",
		"test_read_format_rar5_multiarchive.part07.rar",
		"test_read_format_rar5_multiarchive.part08.rar",
		NULL
	};

	PROLOGUE_MULTI(reffiles);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("home/antek/temp/build/unrar5/libarchive/bin/bsdcat_test", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("home/antek/temp/build/unrar5/libarchive/bin/bsdtar_test", archive_entry_pathname(ae));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_multiarchive_skip_all_but_first)
{
	const char* reffiles[] = {
		"test_read_format_rar5_multiarchive.part01.rar",
		"test_read_format_rar5_multiarchive.part02.rar",
		"test_read_format_rar5_multiarchive.part03.rar",
		"test_read_format_rar5_multiarchive.part04.rar",
		"test_read_format_rar5_multiarchive.part05.rar",
		"test_read_format_rar5_multiarchive.part06.rar",
		"test_read_format_rar5_multiarchive.part07.rar",
		"test_read_format_rar5_multiarchive.part08.rar",
		NULL
	};

	PROLOGUE_MULTI(reffiles);
	assertA(0 == archive_read_next_header(a, &ae));
	assertA(0 == extract_one(a, ae, 0x35277473));
	assertA(0 == archive_read_next_header(a, &ae));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_multiarchive_skip_all_but_second)
{
	const char* reffiles[] = {
		"test_read_format_rar5_multiarchive.part01.rar",
		"test_read_format_rar5_multiarchive.part02.rar",
		"test_read_format_rar5_multiarchive.part03.rar",
		"test_read_format_rar5_multiarchive.part04.rar",
		"test_read_format_rar5_multiarchive.part05.rar",
		"test_read_format_rar5_multiarchive.part06.rar",
		"test_read_format_rar5_multiarchive.part07.rar",
		"test_read_format_rar5_multiarchive.part08.rar",
		NULL
	};

	PROLOGUE_MULTI(reffiles);
	assertA(0 == archive_read_next_header(a, &ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertA(0 == extract_one(a, ae, 0xE59665F8));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_blake2)
{
	const la_ssize_t proper_size = 814;
	uint8_t buf[814];

	PROLOGUE("test_read_format_rar5_blake2.rar");
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(proper_size, archive_entry_size(ae));

	/* Should blake2 calculation fail, we'll get a failure return
	 * value from archive_read_data(). */

	assertA(proper_size == archive_read_data(a, buf, proper_size));

	/* To be extra pedantic, let's also check crc32 of the poem. */
	assertEqualInt(crc32(0, buf, proper_size), 0x7E5EC49E);

	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_arm_filter)
{
	/* This test unpacks a file that uses an ARM filter. The DELTA
	 * and X86 filters are tested implicitly in the "multiarchive_skip"
	 * test. */

	const la_ssize_t proper_size = 90808;
	uint8_t buf[90808];

	PROLOGUE("test_read_format_rar5_arm.rar");
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(proper_size, archive_entry_size(ae));
	assertA(proper_size == archive_read_data(a, buf, proper_size));

	/* Yes, RARv5 unpacker itself should calculate the CRC, but in case
	 * the DONT_FAIL_ON_CRC_ERROR define option is enabled during compilation,
	 * let's still fail the test if the unpacked data is wrong. */
	assertEqualInt(crc32(0, buf, proper_size), 0x886F91EB);

	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_stored_skip_all)
{
	const char* fname = "test_read_format_rar5_stored_manyfiles.rar";

	PROLOGUE(fname);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("make_uue.tcl", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("cebula.txt", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_stored_skip_in_part)
{
	const char* fname = "test_read_format_rar5_stored_manyfiles.rar";
	char buf[6];

	/* Skip first, extract in part rest. */

	PROLOGUE(fname);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("make_uue.tcl", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("cebula.txt", archive_entry_pathname(ae));
	assertA(6 == archive_read_data(a, buf, 6));
	assertEqualInt(0, memcmp(buf, "Cebula", 6));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(4 == archive_read_data(a, buf, 4));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_stored_skip_all_but_first)
{
	const char* fname = "test_read_format_rar5_stored_manyfiles.rar";
	char buf[405];

	/* Extract first, skip rest. */

	PROLOGUE(fname);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("make_uue.tcl", archive_entry_pathname(ae));
	assertA(405 == archive_read_data(a, buf, sizeof(buf)));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("cebula.txt", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_stored_skip_all_in_part)
{
	const char* fname = "test_read_format_rar5_stored_manyfiles.rar";
	char buf[4];

	/* Extract in part all */

	PROLOGUE(fname);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("make_uue.tcl", archive_entry_pathname(ae));
	assertA(4 == archive_read_data(a, buf, 4));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("cebula.txt", archive_entry_pathname(ae));
	assertA(4 == archive_read_data(a, buf, 4));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(4 == archive_read_data(a, buf, 4));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_multiarchive_solid_extr_all)
{
	const char* reffiles[] = {
		"test_read_format_rar5_multiarchive_solid.part01.rar",
		"test_read_format_rar5_multiarchive_solid.part02.rar",
		"test_read_format_rar5_multiarchive_solid.part03.rar",
		"test_read_format_rar5_multiarchive_solid.part04.rar",
		NULL
	};

	PROLOGUE_MULTI(reffiles);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("cebula.txt", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x7E5EC49E));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x7cca70cd));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x7e13b2c6));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0xf166afcb));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x9fb123d9));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x10c43ed4));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test5.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0xb9d155f2));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test6.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x36a448ff));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("elf-Linux-ARMv7-ls", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x886F91EB));

	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_multiarchive_solid_skip_all)
{
	const char* reffiles[] = {
		"test_read_format_rar5_multiarchive_solid.part01.rar",
		"test_read_format_rar5_multiarchive_solid.part02.rar",
		"test_read_format_rar5_multiarchive_solid.part03.rar",
		"test_read_format_rar5_multiarchive_solid.part04.rar",
		NULL
	};

	PROLOGUE_MULTI(reffiles);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("cebula.txt", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test5.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test6.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("elf-Linux-ARMv7-ls", archive_entry_pathname(ae));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_multiarchive_solid_skip_all_but_first)
{
	const char* reffiles[] = {
		"test_read_format_rar5_multiarchive_solid.part01.rar",
		"test_read_format_rar5_multiarchive_solid.part02.rar",
		"test_read_format_rar5_multiarchive_solid.part03.rar",
		"test_read_format_rar5_multiarchive_solid.part04.rar",
		NULL
	};

	PROLOGUE_MULTI(reffiles);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("cebula.txt", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x7E5EC49E));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test5.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test6.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("elf-Linux-ARMv7-ls", archive_entry_pathname(ae));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

/* "skip_all_but_scnd" -> am I hitting the test name limit here after
 * expansion of "scnd" to "second"? */

DEFINE_TEST(test_read_format_rar5_multiarchive_solid_skip_all_but_scnd)
{
	const char* reffiles[] = {
		"test_read_format_rar5_multiarchive_solid.part01.rar",
		"test_read_format_rar5_multiarchive_solid.part02.rar",
		"test_read_format_rar5_multiarchive_solid.part03.rar",
		"test_read_format_rar5_multiarchive_solid.part04.rar",
		NULL
	};

	PROLOGUE_MULTI(reffiles);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("cebula.txt", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x7CCA70CD));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test5.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test6.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("elf-Linux-ARMv7-ls", archive_entry_pathname(ae));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_multiarchive_solid_skip_all_but_third)
{
	const char* reffiles[] = {
		"test_read_format_rar5_multiarchive_solid.part01.rar",
		"test_read_format_rar5_multiarchive_solid.part02.rar",
		"test_read_format_rar5_multiarchive_solid.part03.rar",
		"test_read_format_rar5_multiarchive_solid.part04.rar",
		NULL
	};

	PROLOGUE_MULTI(reffiles);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("cebula.txt", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x7E13B2C6));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test5.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test6.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("elf-Linux-ARMv7-ls", archive_entry_pathname(ae));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_multiarchive_solid_skip_all_but_last)
{
	const char* reffiles[] = {
		"test_read_format_rar5_multiarchive_solid.part01.rar",
		"test_read_format_rar5_multiarchive_solid.part02.rar",
		"test_read_format_rar5_multiarchive_solid.part03.rar",
		"test_read_format_rar5_multiarchive_solid.part04.rar",
		NULL
	};

	PROLOGUE_MULTI(reffiles);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("cebula.txt", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test5.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test6.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("elf-Linux-ARMv7-ls", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x886F91EB));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_solid_skip_all)
{
	const char* reffile = "test_read_format_rar5_solid.rar";

	/* Skip all */

	PROLOGUE(reffile);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test5.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test6.bin", archive_entry_pathname(ae));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_solid_skip_all_but_first)
{
	const char* reffile = "test_read_format_rar5_solid.rar";

	/* Extract first, skip rest */

	PROLOGUE(reffile);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x7CCA70CD));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test5.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test6.bin", archive_entry_pathname(ae));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_solid_skip_all_but_second)
{
	const char* reffile = "test_read_format_rar5_solid.rar";

	/* Skip first, extract second, skip rest */

	PROLOGUE(reffile);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x7E13B2C6));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test5.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test6.bin", archive_entry_pathname(ae));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_solid_skip_all_but_last)
{
	const char* reffile = "test_read_format_rar5_solid.rar";

	/* Skip all but last, extract last */

	PROLOGUE(reffile);
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test5.bin", archive_entry_pathname(ae));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test6.bin", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0x36A448FF));
	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_extract_win32)
{
	PROLOGUE("test_read_format_rar5_win32.rar");
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("testdir", archive_entry_pathname(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFDIR | 0755);
	assertA(0 == extract_one(a, ae, 0));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertA(0 == extract_one(a, ae, 0x7CCA70CD));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test1.bin", archive_entry_pathname(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertA(0 == extract_one(a, ae, 0x7E13B2C6));
	assertA(0 == archive_read_next_header(a, &ae));
	/* Read only file */
	assertEqualString("test2.bin", archive_entry_pathname(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0444);
	assertA(0 == extract_one(a, ae, 0xF166AFCB));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test3.bin", archive_entry_pathname(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertA(0 == extract_one(a, ae, 0x9FB123D9));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test4.bin", archive_entry_pathname(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertA(0 == extract_one(a, ae, 0x10C43ED4));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test5.bin", archive_entry_pathname(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertA(0 == extract_one(a, ae, 0xB9D155F2));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test6.bin", archive_entry_pathname(ae));
	assertEqualInt(archive_entry_mode(ae), AE_IFREG | 0644);
	assertA(0 == extract_one(a, ae, 0x36A448FF));
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_block_by_block)
{
	/* This test uses strange buffer sizes intentionally. */

	struct archive_entry *ae;
	struct archive *a;
	uint8_t buf[173];
	int bytes_read;
	uint32_t computed_crc = 0;

	extract_reference_file("test_read_format_rar5_compressed.rar");
	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_filter_all(a));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_open_filename(a, "test_read_format_rar5_compressed.rar", 130));
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.bin", archive_entry_pathname(ae));
	assertEqualInt(1200, archive_entry_size(ae));

	/* File size is 1200 bytes, we're reading it using a buffer of 173 bytes.
	 * Libarchive is configured to use a buffer of 130 bytes. */

	while(1) {
		/* archive_read_data should return one of:
		 * a) 0, if there is no more data to be read,
		 * b) negative value, if there was an error,
		 * c) positive value, meaning how many bytes were read.
		 */

		bytes_read = archive_read_data(a, buf, sizeof(buf));
		assertA(bytes_read >= 0);
		if(bytes_read <= 0)
			break;

		computed_crc = crc32(computed_crc, buf, bytes_read);
	}

	assertEqualInt(computed_crc, 0x7CCA70CD);
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_owner)
{
	const int DATA_SIZE = 5;
	uint8_t buff[5];

	PROLOGUE("test_read_format_rar5_owner.rar");

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("root.txt", archive_entry_pathname(ae));
	assertEqualString("root", archive_entry_uname(ae));
	assertEqualString("wheel", archive_entry_gname(ae));
	assertA((int) archive_entry_mtime(ae) > 0);
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("nobody.txt", archive_entry_pathname(ae));
	assertEqualString("nobody", archive_entry_uname(ae));
	assertEqualString("nogroup", archive_entry_gname(ae));
	assertA((int) archive_entry_mtime(ae) > 0);
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("numeric.txt", archive_entry_pathname(ae));
	assertEqualInt(9999, archive_entry_uid(ae));
	assertEqualInt(8888, archive_entry_gid(ae));
	assertA((int) archive_entry_mtime(ae) > 0);
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));

	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_symlink)
{
	const int DATA_SIZE = 5;
	uint8_t buff[5];

	PROLOGUE("test_read_format_rar5_symlink.rar");

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("file.txt", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertA((int) archive_entry_mtime(ae) > 0);
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("symlink.txt", archive_entry_pathname(ae));
	assertEqualInt(AE_IFLNK, archive_entry_filetype(ae));
	assertEqualString("file.txt", archive_entry_symlink(ae));
	assertEqualInt(AE_SYMLINK_TYPE_FILE, archive_entry_symlink_type(ae));
	assertA(0 == archive_read_data(a, NULL, archive_entry_size(ae)));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("dirlink", archive_entry_pathname(ae));
	assertEqualInt(AE_IFLNK, archive_entry_filetype(ae));
	assertEqualString("dir", archive_entry_symlink(ae));
	assertEqualInt(AE_SYMLINK_TYPE_DIRECTORY, archive_entry_symlink_type(ae));
	assertA(0 == archive_read_data(a, NULL, archive_entry_size(ae)));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("dir", archive_entry_pathname(ae));
	assertEqualInt(AE_IFDIR, archive_entry_filetype(ae));
	assertA(0 == archive_read_data(a, NULL, archive_entry_size(ae)));

	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_hardlink)
{
	const int DATA_SIZE = 5;
	uint8_t buff[5];

	PROLOGUE("test_read_format_rar5_hardlink.rar");

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("file.txt", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertA((int) archive_entry_mtime(ae) > 0);
	assertEqualInt(DATA_SIZE, archive_entry_size(ae));
	assertA(DATA_SIZE == archive_read_data(a, buff, DATA_SIZE));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("hardlink.txt", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualString("file.txt", archive_entry_hardlink(ae));
	assertA(0 == archive_read_data(a, NULL, archive_entry_size(ae)));

	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_extra_field_version)
{
	PROLOGUE("test_read_format_rar5_extra_field_version.rar");

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("bin/2to3;1", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0xF24181B7));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("bin/2to3", archive_entry_pathname(ae));
	assertA(0 == extract_one(a, ae, 0xF24181B7));

	assertA(ARCHIVE_EOF == archive_read_next_header(a, &ae));

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_readtables_overflow)
{
	uint8_t buf[16];

	PROLOGUE("test_read_format_rar5_readtables_overflow.rar");

	/* This archive is invalid. However, processing it shouldn't cause any
	 * buffer overflow errors during reading rar5 tables. */

	(void) archive_read_next_header(a, &ae);
	(void) archive_read_data(a, buf, sizeof(buf));
	(void) archive_read_next_header(a, &ae);

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_leftshift1)
{
	uint8_t buf[16];

	PROLOGUE("test_read_format_rar5_leftshift1.rar");

	/* This archive is invalid. However, processing it shouldn't cause any
	 * errors related to undefined operations when using -fsanitize. */

	(void) archive_read_next_header(a, &ae);
	(void) archive_read_data(a, buf, sizeof(buf));
	(void) archive_read_next_header(a, &ae);

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_leftshift2)
{
	uint8_t buf[16];

	PROLOGUE("test_read_format_rar5_leftshift2.rar");

	/* This archive is invalid. However, processing it shouldn't cause any
	 * errors related to undefined operations when using -fsanitize. */

	(void) archive_read_next_header(a, &ae);
	(void) archive_read_data(a, buf, sizeof(buf));
	(void) archive_read_next_header(a, &ae);

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_truncated_huff)
{
	uint8_t buf[16];

	PROLOGUE("test_read_format_rar5_truncated_huff.rar");

	/* This archive is invalid. However, processing it shouldn't cause any
	 * errors related to undefined operations when using -fsanitize. */

	(void) archive_read_next_header(a, &ae);
	(void) archive_read_data(a, buf, sizeof(buf));
	(void) archive_read_next_header(a, &ae);

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_invalid_dict_reference)
{
	uint8_t buf[16];

	PROLOGUE("test_read_format_rar5_invalid_dict_reference.rar");

	/* This test should fail on parsing the header. */
	assertA(archive_read_next_header(a, &ae) != ARCHIVE_OK);

	/* This archive is invalid. However, processing it shouldn't cause any
	 * errors related to buffer underflow when using -fsanitize. */
	assertA(archive_read_data(a, buf, sizeof(buf)) <= 0);

	/* This test only cares about not returning success here. */
	assertA(ARCHIVE_OK != archive_read_next_header(a, &ae));

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_distance_overflow)
{
	uint8_t buf[16];

	PROLOGUE("test_read_format_rar5_distance_overflow.rar");

	/* This archive is invalid. However, processing it shouldn't cause any
	 * errors related to variable overflows when using -fsanitize. */

	(void) archive_read_next_header(a, &ae);
	(void) archive_read_data(a, buf, sizeof(buf));
	(void) archive_read_next_header(a, &ae);

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_nonempty_dir_stream)
{
	uint8_t buf[16];

	PROLOGUE("test_read_format_rar5_nonempty_dir_stream.rar");

	/* This archive is invalid. However, processing it shouldn't cause any
	 * errors related to buffer overflows when using -fsanitize. */

	(void) archive_read_next_header(a, &ae);
	(void) archive_read_data(a, buf, sizeof(buf));
	(void) archive_read_next_header(a, &ae);

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_fileattr)
{
	unsigned long set, clear, flag;

	flag = 0;

	PROLOGUE("test_read_format_rar5_fileattr.rar");

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(archive_entry_mode(ae), 0444 | AE_IFREG);
	assertEqualString("readonly.txt", archive_entry_pathname(ae));
	assertEqualString("rdonly", archive_entry_fflags_text(ae));
	archive_entry_fflags(ae, &set, &clear);
#if defined(__FreeBSD__)
	flag = UF_READONLY;
#elif defined(_WIN32) && !defined(CYGWIN)
	flag = FILE_ATTRIBUTE_READONLY;
#endif
	assertEqualInt(flag, set & flag);

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(archive_entry_mode(ae), 0644 | AE_IFREG);
	assertEqualString("hidden.txt", archive_entry_pathname(ae));
	assertEqualString("hidden", archive_entry_fflags_text(ae));
	archive_entry_fflags(ae, &set, &clear);
#if defined(__FreeBSD__)
	flag = UF_HIDDEN;
#elif defined(_WIN32) && !defined(CYGWIN)
	flag = FILE_ATTRIBUTE_HIDDEN;
#endif
	assertEqualInt(flag, set & flag);

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(archive_entry_mode(ae), 0644 | AE_IFREG);
	assertEqualString("system.txt", archive_entry_pathname(ae));
	assertEqualString("system", archive_entry_fflags_text(ae));
	archive_entry_fflags(ae, &set, &clear);
#if defined(__FreeBSD__)
	flag = UF_SYSTEM;;
#elif defined(_WIN32) && !defined(CYGWIN)
	flag = FILE_ATTRIBUTE_SYSTEM;
#endif
	assertEqualInt(flag, set & flag);

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(archive_entry_mode(ae), 0444 | AE_IFREG);
	assertEqualString("ro_hidden.txt", archive_entry_pathname(ae));
	assertEqualString("rdonly,hidden", archive_entry_fflags_text(ae));
	archive_entry_fflags(ae, &set, &clear);
#if defined(__FreeBSD__)
	flag = UF_READONLY | UF_HIDDEN;
#elif defined(_WIN32) && !defined(CYGWIN)
	flag = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN;
#endif
	assertEqualInt(flag, set & flag);

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(archive_entry_mode(ae), 0555 | AE_IFDIR);
	assertEqualString("dir_readonly", archive_entry_pathname(ae));
	assertEqualString("rdonly", archive_entry_fflags_text(ae));
	archive_entry_fflags(ae, &set, &clear);
#if defined(__FreeBSD__)
	flag = UF_READONLY;
#elif defined(_WIN32) && !defined(CYGWIN)
	flag = FILE_ATTRIBUTE_READONLY;
#endif
	assertEqualInt(flag, set & flag);

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(archive_entry_mode(ae), 0755 | AE_IFDIR);
	assertEqualString("dir_hidden", archive_entry_pathname(ae));
	assertEqualString("hidden", archive_entry_fflags_text(ae));
	archive_entry_fflags(ae, &set, &clear);
#if defined(__FreeBSD__)
	flag = UF_HIDDEN;
#elif defined(_WIN32) && !defined(CYGWIN)
	flag = FILE_ATTRIBUTE_HIDDEN;
#endif
	assertEqualInt(flag, set & flag);

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(archive_entry_mode(ae), 0755 | AE_IFDIR);
	assertEqualString("dir_system", archive_entry_pathname(ae));
	assertEqualString("system", archive_entry_fflags_text(ae));
	archive_entry_fflags(ae, &set, &clear);
#if defined(__FreeBSD__)
	flag = UF_SYSTEM;
#elif defined(_WIN32) && !defined(CYGWIN)
	flag = FILE_ATTRIBUTE_SYSTEM;
#endif
	assertEqualInt(flag, set & flag);

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(archive_entry_mode(ae), 0555 | AE_IFDIR);
	assertEqualString("dir_rohidden", archive_entry_pathname(ae));
	assertEqualString("rdonly,hidden", archive_entry_fflags_text(ae));
	archive_entry_fflags(ae, &set, &clear);
#if defined(__FreeBSD__)
	flag = UF_READONLY | UF_HIDDEN;
#elif defined(_WIN32) && !defined(CYGWIN)
	flag = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN;
#endif
	assertEqualInt(flag, set & flag);

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_different_window_size)
{
	char buf[4096];
	PROLOGUE("test_read_format_rar5_different_window_size.rar");

	/* Return codes of those calls are ignored, because this sample file
	 * is invalid. However, the unpacker shouldn't produce any SIGSEGV
	 * errors during processing. */

	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, sizeof(buf))) {}

	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, sizeof(buf))) {}

	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, sizeof(buf))) {}

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_window_buf_and_size_desync)
{
	/* oss fuzz 30442 */

	char buf[4096];
	PROLOGUE("test_read_format_rar5_window_buf_and_size_desync.rar");

	/* Return codes of those calls are ignored, because this sample file
	 * is invalid. However, the unpacker shouldn't produce any SIGSEGV
	 * errors during processing. */

	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, 46)) {}

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_arm_filter_on_window_boundary)
{
	char buf[4096];
	PROLOGUE("test_read_format_rar5_arm_filter_on_window_boundary.rar");

	/* Return codes of those calls are ignored, because this sample file
	 * is invalid. However, the unpacker shouldn't produce any SIGSEGV
	 * errors during processing. */

	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, sizeof(buf))) {}

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_different_solid_window_size)
{
	char buf[4096];
	PROLOGUE("test_read_format_rar5_different_solid_window_size.rar");

	/* Return codes of those calls are ignored, because this sample file
	 * is invalid. However, the unpacker shouldn't produce any SIGSEGV
	 * errors during processing. */

	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, sizeof(buf))) {}

	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, sizeof(buf))) {}

	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, sizeof(buf))) {}

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_different_winsize_on_merge)
{
	char buf[4096];
	PROLOGUE("test_read_format_rar5_different_winsize_on_merge.rar");

	/* Return codes of those calls are ignored, because this sample file
	 * is invalid. However, the unpacker shouldn't produce any SIGSEGV
	 * errors during processing. */

	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, sizeof(buf))) {}

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_block_size_is_too_small)
{
	char buf[4096];
	PROLOGUE("test_read_format_rar5_block_size_is_too_small.rar");

	/* This file is damaged, so those functions should return failure.
	 * Additionally, SIGSEGV shouldn't be raised during execution
	 * of those functions. */

	assertA(archive_read_next_header(a, &ae) != ARCHIVE_OK);
	assertA(archive_read_data(a, buf, sizeof(buf)) <= 0);

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_sfx)
{
	struct archive *a;
	struct archive_entry *ae;
	int bs = 10240;
	char buff[32];
	const char reffile[] = "test_read_format_rar5_sfx.exe";
	const char test_txt[] = "123";
	int size = sizeof(test_txt) - 1;

	extract_reference_file(reffile);
	assert((a = archive_read_new()) != NULL);
	assertA(0 == archive_read_support_filter_all(a));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_open_filename(a, reffile, bs));

	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualString("test.txt.txt", archive_entry_pathname(ae));

	assertA(size == archive_read_data(a, buff, size));
	assertEqualMem(buff, test_txt, size);
	
	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_decode_number_out_of_bounds_read)
{
	/* oss fuzz 30448 */

	char buf[4096];
	PROLOGUE("test_read_format_rar5_decode_number_out_of_bounds_read.rar");

	/* Return codes of those calls are ignored, because this sample file
	 * is invalid. However, the unpacker shouldn't produce any SIGSEGV
	 * errors during processing. */

	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, sizeof(buf))) {}

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_bad_window_size_in_multiarchive_file)
{
	/* oss fuzz 30459 */

	char buf[4096];
	PROLOGUE("test_read_format_rar5_bad_window_sz_in_mltarc_file.rar");

	/* This file is damaged, so those functions should return failure.
	 * Additionally, SIGSEGV shouldn't be raised during execution
	 * of those functions. */

	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, sizeof(buf))) {}
	(void) archive_read_next_header(a, &ae);
	while(0 < archive_read_data(a, buf, sizeof(buf))) {}

	EPILOGUE();
}

DEFINE_TEST(test_read_format_rar5_read_data_block_uninitialized_offset)
{
	const void *buf;
	size_t size;
	la_int64_t offset;

	PROLOGUE("test_read_format_rar5_compressed.rar");
	assertA(0 == archive_read_next_header(a, &ae));

	/* A real code may pass a pointer to an uninitialized variable as an offset
	 * output argument. Here we want to check this situation. But because
	 * relying on a value of an uninitialized variable in a test is not a good
	 * idea, let's pretend that 0xdeadbeef is a random value of the
	 * uninitialized variable. */
	offset = 0xdeadbeef;
	assertEqualInt(ARCHIVE_OK, archive_read_data_block(a, &buf, &size, &offset));
	/* The test archive doesn't contain a sparse file. And because of that, here
	 * we assume that the first returned offset should be 0. */
	assertEqualInt(0, offset);

	EPILOGUE();
}
