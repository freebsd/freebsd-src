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
 * Replay a fuzzer binary through the MTREE writer, matching the protocol
 * in fuzzers/custom/fuzz_writer_mtree.cc.
 */
DEFINE_TEST(test_write_format_mtree_null_deref)
{
	const char *refname = "test_write_format_mtree_null_deref.bin";
	FILE *f;
	uint8_t raw[16384];
	size_t rawsize;
	struct fuzz_consumer consumer;
	uint8_t opts, num_entries;
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
	if (!assert(rawsize >= 4))
		return;

	fuzz_consumer_init(&consumer, raw, rawsize);
	opts = fuzz_consume_byte(&consumer);
	num_entries = (fuzz_consume_byte(&consumer) % 8) + 1;

	a = archive_write_new();
	if (!assert(a != NULL))
		return;

	if (opts & 0x01)
		archive_write_set_format_mtree_classic(a);
	else
		archive_write_set_format_mtree(a);

	if (opts & 0x02)
		archive_write_set_options(a, "mtree:all");
	if (opts & 0x04)
		archive_write_set_options(a, "mtree:use-set");
	if (opts & 0x08)
		archive_write_set_options(a, "mtree:indent");
	if (opts & 0x10)
		archive_write_set_options(a, "mtree:dironly");

	out_buf = malloc(256 * 1024);
	if (!assert(out_buf != NULL)) {
		archive_write_free(a);
		return;
	}
	if (archive_write_open_memory(a, out_buf, 256 * 1024, &used)
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
			name = "file.txt";
		archive_entry_set_pathname(entry, name);

		ftype = fuzz_consume_byte(&consumer) % 5;
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
			archive_entry_set_filetype(entry, AE_IFBLK);
			archive_entry_set_perm(entry, 0600);
			archive_entry_set_rdev(entry,
			    fuzz_consume_u16(&consumer));
			break;
		case 4:
			archive_entry_set_filetype(entry, AE_IFIFO);
			archive_entry_set_perm(entry, 0644);
			break;
		}

		archive_entry_set_uid(entry, fuzz_consume_byte(&consumer));
		archive_entry_set_gid(entry, fuzz_consume_byte(&consumer));
		archive_entry_set_mtime(entry,
		    1700000000 + fuzz_consume_u16(&consumer), 0);
		archive_entry_set_uname(entry, "user");
		archive_entry_set_gname(entry, "group");

		if (fuzz_consumer_remaining(&consumer) > 1 &&
		    (fuzz_consume_byte(&consumer) & 0x01))
			archive_entry_copy_fflags_text(entry, "uappnd,uchg");

		if (ftype == 0) {
			file_size = fuzz_consume_byte(&consumer) % 128;
			archive_entry_set_size(entry, file_size);
		}

		if (archive_write_header(a, entry) != ARCHIVE_OK)
			continue;

		if (file_size > 0 && fuzz_consumer_remaining(&consumer) > 0) {
			size_t to_write = file_size;
			uint8_t data[128];
			if (to_write > fuzz_consumer_remaining(&consumer))
				to_write = fuzz_consumer_remaining(&consumer);
			fuzz_consume_bytes(&consumer, data, to_write);
			archive_write_data(a, data, to_write);
		}
	}

	archive_entry_free(entry);
	/* Close triggers tree traversal; must not crash. */
	archive_write_close(a);
	archive_write_free(a);
	free(out_buf);
}

DEFINE_TEST(test_write_format_mtree_no_set_symlink)
{
	struct archive *a;
	size_t buffsize = 4096;
	char *buff;
	size_t used;
	assert((buff = malloc(buffsize)) != NULL);
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK,
		archive_write_open_memory(a, buff, buffsize, &used));

	struct archive_entry *ae;
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "./badlink"),
	archive_entry_set_filetype(ae, AE_IFLNK);
	archive_entry_set_perm(ae, 0777);
	/* archive_entry_set_symlink(ae, "target"); (omitted) */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	free(buff);
}

DEFINE_TEST(test_write_format_mtree_reg_dot_root)
{
	struct archive *a;
	size_t buffsize = 4096;
	char *buff;
	size_t used;
	assert((buff = malloc(buffsize)) != NULL);
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK,
		archive_write_open_memory(a, buff, buffsize, &used));

	struct archive_entry *ae;
	/* Entry 1: "." with filetype AE_IFREG (not AE_IFDIR). */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "."),
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, 0644);
	assertEqualIntA(a, ARCHIVE_FAILED, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Entry 2: any child path. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "./foo"),
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, 0644);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	free(buff);
}
