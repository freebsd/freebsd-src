/*-
 * Copyright (c) 2026 François Degros
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
 * A minimal ZIP archive with a single empty-ish entry ("ntfs.txt") whose
 * local and central extra data both carry an 0x000A "NTFS" extra field
 * (reserved(4) + Tag(1)/Size(24): Mtime, Atime, Ctime as 8-byte Windows
 * FILETIME values), and no 0x5455 "UT" field. This exercises reading
 * that field directly, rather than through libarchive's own writer,
 * since libarchive doesn't write 0x000A.
 *
 * The three FILETIME values encode, respectively:
 *   Mtime: 2024-03-15T10:20:30.123456000Z
 *   Atime: 2024-03-16T11:21:31.234567000Z
 *   Ctime: 2024-03-14T09:19:29.345678000Z (Windows creation time, which
 *          is why it must map to archive_entry_birthtime(), not ctime)
 */
static const unsigned char archive_ntfs_timestamp[] = {
/* --- local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,          /* version needed to extract: 2.0 */
	0x00, 0x00,          /* general purpose bit flag */
	0x00, 0x00,          /* compression method: stored */
	0x00, 0x00,          /* last mod file time */
	0x00, 0x00,          /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,          /* file name length = 8 */
	0x24, 0x00,          /* extra field length = 36 */
	0x6e, 0x74, 0x66, 0x73, 0x2e, 0x74, 0x78, 0x74, /* "ntfs.txt" */
/* extra field 0x000a "NTFS" */
	0x0a, 0x00,          /* extra field ID = 0x000a */
	0x20, 0x00,          /* extra field size = 32 */
	0x00, 0x00, 0x00, 0x00, /* reserved */
	0x01, 0x00,          /* NTFS attribute tag 1 */
	0x18, 0x00,          /* NTFS attribute 1 size */
	0x80, 0x41, 0xfd, 0x67, 0xc2, 0x76, 0xda, 0x01, /* Mtime FILETIME */
	0x46, 0x3a, 0x98, 0x18, 0x94, 0x77, 0xda, 0x01, /* Atime FILETIME */
	0x8c, 0x25, 0x95, 0xb7, 0xf0, 0x75, 0xda, 0x01, /* Ctime -> birthtime */
	0x68, 0x69, 0x0a,    /* "hi\n" */
/* --- central directory header --- */
	0x50, 0x4b, 0x01, 0x02, /* central directory header signature */
	0x14, 0x00,          /* version made by: 2.0, MS-DOS */
	0x14, 0x00,          /* version needed to extract: 2.0 */
	0x00, 0x00,          /* general purpose bit flag */
	0x00, 0x00,          /* compression method: stored */
	0x00, 0x00,          /* last mod file time */
	0x00, 0x00,          /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,          /* file name length = 8 */
	0x24, 0x00,          /* extra field length = 36 */
	0x00, 0x00,          /* file comment length */
	0x00, 0x00,          /* disk number start */
	0x00, 0x00,          /* internal file attributes */
	0x00, 0x00, 0xa4, 0x81, /* external file attributes: -rw-r--r-- */
	0x00, 0x00, 0x00, 0x00, /* relative offset of local header = 0 */
	0x6e, 0x74, 0x66, 0x73, 0x2e, 0x74, 0x78, 0x74, /* "ntfs.txt" */
/* extra field 0x000a "NTFS" */
	0x0a, 0x00,          /* extra field ID = 0x000a */
	0x20, 0x00,          /* extra field size = 32 */
	0x00, 0x00, 0x00, 0x00, /* reserved */
	0x01, 0x00,          /* NTFS attribute tag 1 */
	0x18, 0x00,          /* NTFS attribute 1 size */
	0x80, 0x41, 0xfd, 0x67, 0xc2, 0x76, 0xda, 0x01, /* Mtime FILETIME */
	0x46, 0x3a, 0x98, 0x18, 0x94, 0x77, 0xda, 0x01, /* Atime FILETIME */
	0x8c, 0x25, 0x95, 0xb7, 0xf0, 0x75, 0xda, 0x01, /* Ctime -> birthtime */
/* --- end of central directory record --- */
	0x50, 0x4b, 0x05, 0x06, /* end of central directory signature */
	0x00, 0x00,          /* number of this disk */
	0x00, 0x00,          /* disk with start of central directory */
	0x01, 0x00,          /* central directory entries on this disk */
	0x01, 0x00,          /* total central directory entries */
	0x5a, 0x00, 0x00, 0x00, /* size of central directory = 90 */
	0x4d, 0x00, 0x00, 0x00, /* offset of central directory = 77 */
	0x00, 0x00,          /* .ZIP file comment length */
};

DEFINE_TEST(test_read_format_zip_ntfs_timestamp)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_ntfs_timestamp,
		sizeof(archive_ntfs_timestamp)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualString("ntfs.txt", archive_entry_pathname(ae));

	assert(archive_entry_mtime_is_set(ae));
	assertEqualInt(1710498030, archive_entry_mtime(ae));
	assertEqualInt(123456000, archive_entry_mtime_nsec(ae));

	assert(archive_entry_atime_is_set(ae));
	assertEqualInt(1710588091, archive_entry_atime(ae));
	assertEqualInt(234567000, archive_entry_atime_nsec(ae));

	/* NTFS "Ctime" is Windows creation time, so it must map to
	 * birthtime, not to POSIX ctime (which ZIP has no source for
	 * here, so it stays unset by this extra field). */
	assert(archive_entry_birthtime_is_set(ae));
	assertEqualInt(1710407969, archive_entry_birthtime(ae));
	assertEqualInt(345678000, archive_entry_birthtime_nsec(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
