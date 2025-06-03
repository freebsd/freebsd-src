/*-
 * Copyright (c) 2003-2023 Tim Kientzle
 * Copyright (c) 2008 Anselm Strauss
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
 * Detailed byte-for-byte verification of the format of a zip archive
 * written in streaming mode with Zip64 extensions enabled.
 */

DEFINE_TEST(test_write_format_zip64_stream)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t used, buffsize = 1000000;
	unsigned long crc;
	unsigned long compressed_size = 0;
	__LA_MODE_T file_perm = 00644;
	int zip_version = 45;
	int zip_compression = 8;
	short file_uid = 10, file_gid = 20;
	unsigned char *buff, *buffend, *p;
	unsigned char *central_header, *local_header, *eocd, *eocd_record;
	unsigned char *extension_start, *extension_end;
	unsigned char *data_start, *data_end;
	char file_data[] = {'1', '2', '3', '4', '5', '6', '7', '8'};
	const char *file_name = "file";

#ifndef HAVE_ZLIB_H
	zip_compression = 0;
#endif

	buff = malloc(buffsize);

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_options(a, "zip:zip64"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_options(a, "zip:experimental"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, file_name);
	archive_entry_set_mode(ae, AE_IFREG | file_perm);
	archive_entry_set_uid(ae, file_uid);
	archive_entry_set_gid(ae, file_gid);
	archive_entry_set_mtime(ae, 0, 0);
	assertEqualInt(0, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(8, archive_write_data(a, file_data, sizeof(file_data)));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	buffend = buff + used;
	dumpfile("constructed.zip", buff, used);

	/* Verify "End of Central Directory" record. */
	/* Get address of end-of-central-directory record. */
	eocd_record = p = buffend - 22; /* Assumes there is no zip comment field. */
	failure("End-of-central-directory begins with PK\\005\\006 signature");
	assertEqualMem(p, "PK\005\006", 4);
	failure("This must be disk 0");
	assertEqualInt(i2le(p + 4), 0);
	failure("Central dir must start on disk 0");
	assertEqualInt(i2le(p + 6), 0);
	failure("All central dir entries are on this disk");
	assertEqualInt(i2le(p + 8), i2le(p + 10));
	eocd = buff + i4le(p + 12) + i4le(p + 16);
	failure("no zip comment");
	assertEqualInt(i2le(p + 20), 0);

	/* Get address of first entry in central directory. */
	central_header = p = buff + i4le(buffend - 6);
	failure("Central file record at offset %u should begin with"
	    " PK\\001\\002 signature",
	    i4le(buffend - 10));

	/* Verify file entry in central directory. */
	assertEqualMem(p, "PK\001\002", 4); /* Signature */
	assertEqualInt(i2le(p + 4), 3 * 256 + zip_version); /* Version made by */
	assertEqualInt(i2le(p + 6), zip_version); /* Version needed to extract */
	assertEqualInt(i2le(p + 8), 8); /* Flags */
	assertEqualInt(i2le(p + 10), zip_compression); /* Compression method */
	assertEqualInt(i2le(p + 12), 0); /* File time */
	assertEqualInt(i2le(p + 14), 33); /* File date */
	crc = bitcrc32(0, file_data, sizeof(file_data));
	assertEqualInt(i4le(p + 16), crc); /* CRC-32 */
	compressed_size = i4le(p + 20);  /* Compressed size */
	assertEqualInt(i4le(p + 24), sizeof(file_data)); /* Uncompressed size */
	assertEqualInt(i2le(p + 28), strlen(file_name)); /* Pathname length */
	/* assertEqualInt(i2le(p + 30), 28); */ /* Extra field length: See below */
	assertEqualInt(i2le(p + 32), 0); /* File comment length */
	assertEqualInt(i2le(p + 34), 0); /* Disk number start */
	assertEqualInt(i2le(p + 36), 0); /* Internal file attrs */
	assertEqualInt(i4le(p + 38) >> 16 & 01777, file_perm); /* External file attrs */
	assertEqualInt(i4le(p + 42), 0); /* Offset of local header */
	assertEqualMem(p + 46, file_name, strlen(file_name)); /* Pathname */
	p = extension_start = central_header + 46 + strlen(file_name);
	extension_end = extension_start + i2le(central_header + 30);

	assertEqualInt(i2le(p), 0x7875);  /* 'ux' extension header */
	assertEqualInt(i2le(p + 2), 11); /* 'ux' size */
	/* TODO: verify 'ux' contents */
	p += 4 + i2le(p + 2);

	assertEqualInt(i2le(p), 0x5455);  /* 'UT' extension header */
	assertEqualInt(i2le(p + 2), 5); /* 'UT' size */
	assertEqualInt(p[4], 1); /* 'UT' flags */
	assertEqualInt(i4le(p + 5), 0); /* 'UT' mtime */
	p += 4 + i2le(p + 2);

	/* Note: We don't expect to see zip64 extension in the central
	 * directory, since the writer knows the actual full size by
	 * the time it is ready to write the central directory and has
	 * no reason to insert it then.  Info-Zip seems to do the same
	 * thing. */

	/* Just in case: Report any extra extensions. */
	while (p < extension_end) {
		failure("Unexpected extension 0x%04X", i2le(p));
		assert(0);
		p += 4 + i2le(p + 2);
	}

	/* Should have run exactly to end of extra data. */
	assertEqualAddress(p, extension_end);

	assertEqualAddress(p, eocd);

	/* After Central dir, we find Zip64 eocd and Zip64 eocd locator. */
	assertEqualMem(p, "PK\006\006", 4); /* Zip64 eocd */
	assertEqualInt(i8le(p + 4), 44); /* We're using v1 Zip64 eocd */
	assertEqualInt(i2le(p + 12), 45); /* Written by Version 4.5 */
	assertEqualInt(i2le(p + 14), 45); /* Needs version 4.5 to extract */
	assertEqualInt(i4le(p + 16), 0); /* This is disk #0 */
	assertEqualInt(i4le(p + 20), 0); /* Dir starts on disk #0 */
	assertEqualInt(i8le(p + 24), 1); /* 1 entry on this disk */
	assertEqualInt(i8le(p + 32), 1); /* 1 entry total */
	assertEqualInt(i8le(p + 40), eocd - central_header); /* size of cd */
	assertEqualInt(i8le(p + 48), central_header - buff); /* start of cd */
	p += 12 + i8le(p + 4);

	assertEqualMem(p, "PK\006\007", 4); /* Zip64 eocd locator */
	assertEqualInt(i4le(p + 4), 0); /* Zip64 eocd is on disk #0 */
	assertEqualInt(i8le(p + 8), eocd - buff); /* Offset of Zip64 eocd */
	assertEqualInt(i4le(p + 16), 1); /* 1 disk */
	p += 20;

	/* Regular EOCD immediately follows Zip64 records. */
	assertEqualAddress(p, eocd_record);

	/* Verify local header of file entry. */
	p = local_header = buff;
	assertEqualMem(p, "PK\003\004", 4); /* Signature */
	assertEqualInt(i2le(p + 4), zip_version); /* Version needed to extract */
	assertEqualInt(i2le(p + 6), 8); /* Flags: bit 3 = length-at-end */
	assertEqualInt(i2le(p + 8), zip_compression); /* Compression method */
	assertEqualInt(i2le(p + 10), 0); /* File time */
	assertEqualInt(i2le(p + 12), 33); /* File date */
	assertEqualInt(i4le(p + 14), 0); /* CRC-32 */
	assertEqualInt(i4le(p + 18), 0); /* Compressed size must be zero for length-at-end */
	assertEqualInt(i4le(p + 22), 0); /* Uncompressed size must be zero for length-at-end */
	assertEqualInt(i2le(p + 26), strlen(file_name)); /* Pathname length */
	assertEqualInt(i2le(p + 28), 37); /* Extra field length */
	assertEqualMem(p + 30, file_name, strlen(file_name)); /* Pathname */
	p = extension_start = local_header + 30 + strlen(file_name);
	extension_end = extension_start + i2le(local_header + 28);

	assertEqualInt(i2le(p), 0x7875);  /* 'ux' extension header */
	assertEqualInt(i2le(p + 2), 11); /* 'ux' size */
	assertEqualInt(p[4], 1); /* 'ux' version */
	assertEqualInt(p[5], 4); /* 'ux' uid size */
	assertEqualInt(i4le(p + 6), file_uid); /* 'Ux' UID */
	assertEqualInt(p[10], 4); /* 'ux' gid size */
	assertEqualInt(i4le(p + 11), file_gid); /* 'Ux' GID */
	p += 4 + i2le(p + 2);

	assertEqualInt(i2le(p), 0x5455);  /* 'UT' extension header */
	assertEqualInt(i2le(p + 2), 5); /* 'UT' size */
	assertEqualInt(p[4], 1); /* 'UT' flags */
	assertEqualInt(i4le(p + 5), 0); /* 'UT' mtime */
	p += 4 + i2le(p + 2);

	assertEqualInt(i2le(p), 0x6c78); /* 'xl' experimental extension header */
	assertEqualInt(i2le(p + 2), 9); /* size */
	assertEqualInt(p[4], 7); /* bitmap of included fields */
	assertEqualInt(i2le(p + 5) >> 8, 3); /* system & version made by */
	assertEqualInt(i2le(p + 7), 0); /* internal file attributes */
	assertEqualInt(i4le(p + 9) >> 16 & 01777, file_perm); /* external file attributes */
	p += 4 + i2le(p + 2);

	/* Just in case: Report any extra extensions. */
	while (p < extension_end) {
		failure("Unexpected extension 0x%04X", i2le(p));
		assert(0);
		p += 4 + i2le(p + 2);
	}

	/* Should have run exactly to end of extra data. */
	assertEqualAddress(p, extension_end);
	data_start = p;

	/* Data descriptor should follow compressed data. */
	while (p < central_header && memcmp(p, "PK\007\010", 4) != 0)
		++p;
	data_end = p;
	assertEqualInt(data_end - data_start, compressed_size);
	assertEqualMem(p, "PK\007\010", 4);
	assertEqualInt(i4le(p + 4), crc); /* CRC-32 */
	assertEqualInt(i8le(p + 8), compressed_size); /* compressed size */
	assertEqualInt(i8le(p + 16), sizeof(file_data)); /* uncompressed size */

	/* Central directory should immediately follow the only entry. */
	assertEqualAddress(p + 24, central_header);

	free(buff);
}
