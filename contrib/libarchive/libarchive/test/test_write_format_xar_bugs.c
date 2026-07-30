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
 * Replay a fuzzer binary through the XAR writer, matching the protocol
 * in fuzzers/custom/fuzz_writer_xar.cc.
 */
static void
replay_xar_writer(const char *refname)
{
	FILE *f;
	uint8_t raw[16384];
	size_t rawsize;
	struct fuzz_consumer consumer;
	uint8_t opts, num_entries;
	struct archive *a;
	struct archive_entry *entry;
	size_t used;
	char *out_buf;
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
	opts = fuzz_consume_byte(&consumer);
	num_entries = (fuzz_consume_byte(&consumer) % 5) + 1;

	a = archive_write_new();
	if (!assert(a != NULL))
		return;

	archive_write_set_format_xar(a);
	archive_write_add_filter_none(a);

	if (opts & 0x01)
		archive_write_set_options(a, "xar:compression=gzip");
	if (opts & 0x02)
		archive_write_set_options(a, "xar:compression=bzip2");
	if (opts & 0x04)
		archive_write_set_options(a, "xar:checksum=sha1");
	if (opts & 0x08)
		archive_write_set_options(a, "xar:checksum=md5");

	out_buf = (char *)malloc(512 * 1024);
	if (!assert(out_buf != NULL)) {
		archive_write_free(a);
		return;
	}
	if (archive_write_open_memory(a, out_buf, 512 * 1024, &used)
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

	for (i = 0; i < num_entries && fuzz_consumer_remaining(&consumer) > 4;
	    i++) {
		const char *path;
		uint8_t ftype;
		uint16_t raw_mode;
		int64_t file_size = 0;

		archive_entry_clear(entry);

		path = fuzz_consume_string(&consumer, 200);
		if (path[0] == '\0')
			path = "file.txt";
		archive_entry_set_pathname(entry, path);

		ftype = fuzz_consume_byte(&consumer) % 4;
		raw_mode = fuzz_consume_u16(&consumer) & 07777;
		switch (ftype) {
		case 0: archive_entry_set_filetype(entry, AE_IFREG); break;
		case 1: archive_entry_set_filetype(entry, AE_IFDIR); break;
		case 2: archive_entry_set_filetype(entry, AE_IFLNK); break;
		default: archive_entry_set_filetype(entry, AE_IFCHR); break;
		}
		archive_entry_set_perm(entry, raw_mode);

		archive_entry_set_uname(entry,
		    fuzz_consume_string(&consumer, 32));
		archive_entry_set_gname(entry,
		    fuzz_consume_string(&consumer, 32));
		archive_entry_set_uid(entry, fuzz_consume_u32(&consumer));
		archive_entry_set_gid(entry, fuzz_consume_u32(&consumer));
		archive_entry_set_mtime(entry,
		    fuzz_consume_i64(&consumer), fuzz_consume_u32(&consumer));
		archive_entry_set_atime(entry,
		    fuzz_consume_i64(&consumer), 0);
		archive_entry_set_ctime(entry,
		    fuzz_consume_i64(&consumer), 0);

		if (ftype == 0) {
			file_size = fuzz_consume_byte(&consumer) % 200;
			archive_entry_set_size(entry, file_size);
		}

		if (ftype == 2)
			archive_entry_set_symlink(entry,
			    fuzz_consume_string(&consumer, 64));

		if (archive_write_header(a, entry) == ARCHIVE_OK
		    && file_size > 0) {
			size_t write_len = (size_t)file_size;
			if (write_len > fuzz_consumer_remaining(&consumer))
				write_len = fuzz_consumer_remaining(&consumer);
			if (write_len > 0) {
				uint8_t file_data[200];
				fuzz_consume_bytes(&consumer, file_data,
				    write_len);
				archive_write_data(a, file_data, write_len);
			}
		}
	}

	archive_entry_free(entry);
	archive_write_close(a);
	archive_write_free(a);
	free(out_buf);
}

DEFINE_TEST(test_write_format_xar_strcpy_overlap)
{
	replay_xar_writer("test_write_format_xar_strcpy_overlap.bin");
}

DEFINE_TEST(test_write_format_xar_underflow)
{
	replay_xar_writer("test_write_format_xar_underflow.bin");
}
