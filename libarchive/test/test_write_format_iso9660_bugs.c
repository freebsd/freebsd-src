/*-
 * Copyright (c) 2025 Tim Kientzle
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
#include "test_fuzz_consumer.h"

#include <stdlib.h>

/*
 * Replay a fuzzer binary through the ISO9660 writer, matching the protocol
 * in fuzzers/custom/fuzz_writer_iso9660.cc.
 */
static void
replay_iso9660_writer(const char *refname)
{
	FILE *f;
	uint8_t raw[32768];
	size_t rawsize;
	struct fuzz_consumer consumer;
	uint8_t opts1, opts2, num_entries;
	struct archive *a;
	struct archive_entry *entry;
	size_t used;
	void *out_buf;
	int i;

	extract_reference_file(refname);
	f = fopen(refname, "rb");
	if (!assert(f != NULL))
		return;
	rawsize = fread(raw, 1, sizeof(raw), f);
	fclose(f);
	if (!assert(rawsize >= 6))
		return;

	fuzz_consumer_init(&consumer, raw, rawsize);
	opts1 = fuzz_consume_byte(&consumer);
	opts2 = fuzz_consume_byte(&consumer);
	num_entries = (fuzz_consume_byte(&consumer) % 6) + 1;

	a = archive_write_new();
	if (!assert(a != NULL))
		return;

	archive_write_set_format_iso9660(a);

	if (opts1 & 0x01)
		archive_write_set_options(a, "iso9660:rockridge");
	if (opts1 & 0x02)
		archive_write_set_options(a, "iso9660:joliet");
	if (opts1 & 0x04)
		archive_write_set_options(a, "iso9660:iso-level=3");
	if (opts1 & 0x08)
		archive_write_set_options(a, "iso9660:iso-level=4");
	if (opts1 & 0x10)
		archive_write_set_options(a, "iso9660:allow-vernum");
	if (opts1 & 0x20)
		archive_write_set_options(a, "iso9660:allow-period");
	if (opts1 & 0x40)
		archive_write_set_options(a, "iso9660:allow-lowercase");
	if (opts1 & 0x80)
		archive_write_set_options(a, "iso9660:allow-multidot");

	if (opts2 & 0x02)
		archive_write_set_options(a,
		    "iso9660:publisher=FuzzPublisher");
	if (opts2 & 0x04)
		archive_write_set_options(a,
		    "iso9660:volume-id=FUZZISO");

	out_buf = malloc(4 * 1024 * 1024);
	if (!assert(out_buf != NULL)) {
		archive_write_free(a);
		return;
	}
	if (archive_write_open_memory(a, out_buf, 4 * 1024 * 1024, &used)
	    != ARCHIVE_OK) {
		archive_write_free(a);
		free(out_buf);
		return;
	}

	entry = archive_entry_new();
	if (!assert(entry != NULL)) {
		archive_write_free(a);
		free(out_buf);
		return;
	}

	for (i = 0; i < num_entries && fuzz_consumer_remaining(&consumer) > 2;
	    i++) {
		const char *name;
		uint8_t ftype;
		uint32_t file_size = 0;

		archive_entry_clear(entry);

		name = fuzz_consume_string(&consumer, 128);
		if (name[0] == '\0')
			name = "FILE.TXT";
		archive_entry_set_pathname(entry, name);

		ftype = fuzz_consume_byte(&consumer) % 4;
		switch (ftype) {
		case 0:
			archive_entry_set_filetype(entry, AE_IFREG);
			archive_entry_set_perm(entry, 0644);
			break;
		case 1:
			archive_entry_set_filetype(entry, AE_IFDIR);
			archive_entry_set_perm(entry, 0755);
			break;
		case 2:
			archive_entry_set_filetype(entry, AE_IFLNK);
			archive_entry_set_perm(entry, 0777);
			archive_entry_set_symlink(entry,
			    fuzz_consume_string(&consumer, 64));
			break;
		case 3:
			archive_entry_set_filetype(entry, AE_IFREG);
			archive_entry_set_perm(entry, 0755);
			break;
		}

		archive_entry_set_uid(entry, 1000);
		archive_entry_set_gid(entry, 1000);
		archive_entry_set_mtime(entry,
		    1700000000 + fuzz_consume_u16(&consumer), 0);
		archive_entry_set_uname(entry, "user");
		archive_entry_set_gname(entry, "group");

		if (ftype == 0 || ftype == 3) {
			file_size = fuzz_consume_u16(&consumer) % 2048;
			archive_entry_set_size(entry, file_size);
		}

		if (archive_write_header(a, entry) != ARCHIVE_OK)
			continue;

		if (file_size > 0 &&
		    fuzz_consumer_remaining(&consumer) > 0) {
			size_t to_write = file_size;
			uint8_t data[2048];
			if (to_write > fuzz_consumer_remaining(&consumer))
				to_write =
				    fuzz_consumer_remaining(&consumer);
			fuzz_consume_bytes(&consumer, data, to_write);
			archive_write_data(a, data, to_write);
		}
	}

	archive_entry_free(entry);
	/* Close triggers path table construction; must not crash. */
	archive_write_close(a);
	archive_write_free(a);
	free(out_buf);
}

DEFINE_TEST(test_write_format_iso9660_null_deref)
{
	replay_iso9660_writer("test_write_format_iso9660_null_deref.bin");
}

DEFINE_TEST(test_write_format_iso9660_underflow)
{
	replay_iso9660_writer("test_write_format_iso9660_underflow.bin");
}

DEFINE_TEST(test_write_format_iso9660_joliet_overflow)
{
	replay_iso9660_writer("test_write_format_iso9660_joliet_overflow.bin");
}

DEFINE_TEST(test_write_format_iso9660_duplicate_identifier_truncation)
{
	const char *names[2];
	struct archive *a;
	struct archive_entry *entry;
	char name1[66], name2[66];
	unsigned char *buff;
	size_t buffsize = 190 * 2048;
	size_t i, used = 0;

	/* These names collide after default Joliet identifier truncation. */
	memset(name1, 'A', sizeof(name1));
	memset(name2, 'A', sizeof(name2));
	name1[0] = name2[0] = '.';
	name1[64] = 'X';
	name2[64] = 'Y';
	name1[65] = name2[65] = '\0';
	names[0] = name1;
	names[1] = name2;

	buff = malloc(buffsize);
	assert(buff != NULL);
	if (buff == NULL)
		return;

	assert((a = archive_write_new()) != NULL);
	if (a == NULL) {
		free(buff);
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_iso9660(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_per_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_bytes_in_last_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));
	for (i = 0; i < 2; i++) {
		entry = archive_entry_new();
		if (!assert(entry != NULL))
			break;
		archive_entry_copy_pathname(entry, names[i]);
		archive_entry_set_mode(entry, AE_IFREG | 0644);
		archive_entry_set_size(entry, 0);
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_write_header(a, entry));
		archive_entry_free(entry);
	}
	failure("ISO9660 writer should resolve duplicate Joliet identifiers "
	    "after truncation");
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	free(buff);
}


DEFINE_TEST(test_write_format_iso9660_symlink)
{
	const size_t buffsize = 512 * 1024;
	unsigned char *buff;
	size_t used = 0;
	char name[1024];
	struct archive *a;
	struct archive_entry *ae;

	assert((buff = malloc(buffsize)) != NULL);
	for (size_t i = 0; i + 1 < sizeof(name); i++) {
		/* Write the archive */
		assert((a = archive_write_new()) != NULL);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_iso9660(a));
		assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
		assertEqualIntA(a, ARCHIVE_OK,
			archive_write_open_memory(a, buff, buffsize, &used));
		assert((ae = archive_entry_new()) != NULL);
		name[i] = 'A';
		name[i + 1] = '\0';
		archive_entry_copy_pathname(ae, name);
		archive_entry_set_filetype(ae, AE_IFLNK);
		archive_entry_set_symlink(ae, "B");
		archive_entry_set_size(ae, 0);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		archive_entry_free(ae);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
		assertEqualInt(ARCHIVE_OK, archive_write_free(a));

		/* Read the archive back */
		assert((a = archive_read_new()) != NULL);
		assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
		assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
		assertEqualIntA(a, ARCHIVE_OK,
			archive_read_open_memory(a, buff, used));

		/* First entry: the root directory '.' */
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualStringA(a, ".", archive_entry_pathname(ae));
		assertEqualIntA(a, AE_IFDIR, archive_entry_filetype(ae));

		/* Second entry: the symbolic link */
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualStringA(a, name, archive_entry_pathname(ae));
		assertEqualIntA(a, AE_IFLNK, archive_entry_filetype(ae));
		assertEqualStringA(a, "B", archive_entry_symlink(ae));
		assertEqualIntA(a, 0, archive_entry_size(ae));

		/* End of archive */
		assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
		assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	}
	free(buff);
}
