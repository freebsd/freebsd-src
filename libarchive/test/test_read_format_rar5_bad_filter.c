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

/*
 * Two-entry RAR5 archive. Entry 1 ("bad.txt") is compressed with a filter
 * of unsupported type 4 (FILTER_AUDIO). Entry 2 ("ok.txt") is stored.
 *
 * Tests that an unsupported filter type in do_uncompress_file returns
 * ARCHIVE_FAILED (not ARCHIVE_FATAL), so the archive is not aborted and
 * the second entry is still readable.
 *
 * RAR5 outer block format: 4-byte LE CRC32 of (vint(hdr_size)+hdr_body),
 * then vint(hdr_size), then hdr_body, then optional data area.
 * Integers are vint: 7 bits per byte, LSB first, high bit = more follows.
 */
DEFINE_TEST(test_read_format_rar5_bad_filter)
{
	static const uint8_t data[] = {
		/* RAR5 signature */
		0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x01, 0x00,

		/* Main archive header block (type=1):
		 *   CRC32, vint(body_size=3), type=1, block_flags=0,
		 *   archive_flags=0 */
		0xC5, 0x1A, 0x33, 0x32, 0x03, 0x01, 0x00, 0x00,

		/* File header block for "bad.txt" (type=2, HFL_DATA flag set):
		 *   CRC32, vint(body_size=17), type=2, HFL_DATA,
		 *   packed_size=23, file_flags=0, unpacked_size=4,
		 *   file_attr=0, compression_info=512 (method=GOOD,
		 *   version=0, window_factor=0), host_os=1,
		 *   name_len=7, "bad.txt" */
		0x88, 0xEC, 0xB0, 0x99, 0x11, 0x02, 0x02, 0x17,
		0x00, 0x04, 0x00, 0x80, 0x04, 0x01, 0x07, 0x62,
		0x61, 0x64, 0x2E, 0x74, 0x78, 0x74,

		/* Compressed data block for "bad.txt" (23 bytes):
		 *
		 * Compressed block header (3 bytes):
		 *   block_flags=0xC2: is_table_present, is_last_block,
		 *                     bf_bit_size=2 (block ends when
		 *                     in_addr==last_byte && bit_addr>=3)
		 *   checksum=0x8C, block_size=20 */
		0xC2, 0x8C, 0x14,

		/* Huffman meta-table (20 nibbles packed in 10 bytes):
		 *   nibble[2]=1, nibble[19]=1, all others 0.
		 *   Two 1-bit meta-symbols:
		 *     code 0 -> sym 2  (store bit-length 2 for next entry)
		 *     code 1 -> sym 19 (fill N+11 zeros, N in next 7 bits) */
		0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x01,

		/* Huffman main table + compressed data (MSB-first bitstream):
		 *
		 * Main table (ld[], 430 entries, 42 bits total):
		 *   skip 97 zeros (sym19, n=86), ld[97]=2 (sym2, 'a'),
		 *   skip 138 zeros, skip 20 zeros,
		 *   ld[256]=2 (sym2, filter token), skip 138+35 zeros.
		 *   Canonical codes: 00->sym97('a'), 01->sym256(filter).
		 *
		 * Compressed data (33 bits):
		 *   01           filter token (sym 256)
		 *   00,00000000  block_start: count=0->1 byte, value=0
		 *   00,00000100  block_length: count=0->1 byte, value=4
		 *   100          filter_type=4 (FILTER_AUDIO, unsupported;
		 *                top 3 bits of a 16-bit read, 3 bits consumed)
		 *   00,00,00,00  four 'a' literals (sym 97, code 00 each) */
		0xD6, 0x7F, 0xC4, 0xBF, 0xE6, 0x10, 0x00, 0x04,
		0x80, 0x00,

		/* File header block for "ok.txt" (type=2, HFL_DATA flag set):
		 *   CRC32, vint(body_size=15), type=2, HFL_DATA,
		 *   packed_size=3, file_flags=0, unpacked_size=3,
		 *   file_attr=0, compression_info=0 (STORE),
		 *   host_os=1, name_len=6, "ok.txt", data "ok\n" */
		0xB1, 0xC3, 0x30, 0x14, 0x0F, 0x02, 0x02, 0x03,
		0x00, 0x03, 0x00, 0x00, 0x01, 0x06, 0x6F, 0x6B,
		0x2E, 0x74, 0x78, 0x74, 0x6F, 0x6B, 0x0A,

		/* End of archive block (type=5):
		 *   CRC32, vint(body_size=2), type=5, block_flags=0 */
		0x39, 0xF9, 0xB2, 0x81, 0x02, 0x05, 0x00,
	};

	struct archive_entry *ae;
	struct archive *a;
	char buf[256];

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_rar5(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, data, sizeof(data)));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("bad.txt", archive_entry_pathname(ae));
	assertA(archive_read_data(a, buf, sizeof(buf)) < 0);
	assertA(archive_error_string(a) != NULL &&
	    strstr(archive_error_string(a), "Unsupported filter type") != NULL);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("ok.txt", archive_entry_pathname(ae));
	assertEqualIntA(a, 3, archive_read_data(a, buf, sizeof(buf)));
	assertEqualMem(buf, "ok\n", 3);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
