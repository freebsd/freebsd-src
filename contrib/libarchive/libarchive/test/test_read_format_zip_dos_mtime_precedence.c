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
 * Regression tests for the Local Header's legacy DOS mtime NOT clobbering
 * a more precise mtime already established from an extra field. Unlike
 * test_read_format_zip_ntfs_precedence.c (which places the winning extra
 * field in the Local Header, alongside a losing coarser one), each archive
 * here places its timestamp extra field *only* in the Central Directory -
 * a Local Header is allowed to omit an extra field the Central Directory
 * carries. That means the seekable reader's second, redundant call to
 * process_extra() on the Local Header's own (here absent) extra data can't
 * mask a broken guard by re-deriving the correct mtime a second time: this
 * is the one scenario where the DOS-clobber guard at
 * zip_read_local_file_header() actually does the work, so it's the one
 * that must be tested directly.
 *
 * Each archive's Local Header carries a deliberately-wrong DOS time/date
 * (DOS_MIN_TIME, 1980-01-01) that must NOT end up as the entry's mtime;
 * the Central-Directory-only extra field gives the correct value:
 *   mtime = 1700000000 (with .100000000 sub-second precision for NTFS)
 */

/* 0x000A "NTFS", Central Directory only. */
static const unsigned char archive_dos_mtime_precedence_ntfs[] = {
/* --- local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time (wrong-on-purpose: DOS_MIN_TIME) */
	0x21, 0x00,             /* last mod file date (wrong-on-purpose: DOS_MIN_TIME) */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x00, 0x00,             /* extra field length = 0 (no extra field here) */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x000a "NTFS" placed in the Central Directory ONLY -
 * the Local Header above has none: this is the scenario the
 * DOS-clobber guard exists for. */
	0x68, 0x69, 0x0a,       /* "hi\n" */
/* --- central directory header --- */
	0x50, 0x4b, 0x01, 0x02, /* central directory header signature */
	0x14, 0x00,             /* version made by: 2.0, MS-DOS */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time (unused: 0x000a "NTFS" wins) */
	0x21, 0x00,             /* last mod file date (unused: 0x000a "NTFS" wins) */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x24, 0x00,             /* extra field length = 36 */
	0x00, 0x00,             /* file comment length */
	0x00, 0x00,             /* disk number start */
	0x00, 0x00,             /* internal file attributes */
	0x00, 0x00, 0xa4, 0x81, /* external file attributes: -rw-r--r-- */
	0x00, 0x00, 0x00, 0x00, /* relative offset of local header = 0 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x000a "NTFS" (only copy: must set mtime) */
	0x0a, 0x00,             /* extra field ID = 0x000a */
	0x20, 0x00,             /* extra field size = 32 */
	0x00, 0x00, 0x00, 0x00, /* reserved */
	0x01, 0x00,             /* NTFS attribute tag 1 */
	0x18, 0x00,             /* NTFS attribute 1 size */
	0x40, 0x42, 0x7c, 0xc6, 0x47, 0x17, 0xda, 0x01, /* Mtime -> 1700000000.100000000 */
	0x80, 0x4e, 0x26, 0x02, 0x48, 0x17, 0xda, 0x01, /* Atime -> 1700000100.200000000 */
	0xc0, 0x5a, 0xd0, 0x3d, 0x48, 0x17, 0xda, 0x01, /* Ctime -> 1700000200.300000000 (-> birthtime) */
/* --- end of central directory record --- */
	0x50, 0x4b, 0x05, 0x06, /* end of central directory signature */
	0x00, 0x00,             /* number of this disk */
	0x00, 0x00,             /* disk with start of central directory */
	0x01, 0x00,             /* central directory entries on this disk */
	0x01, 0x00,             /* total central directory entries */
	0x5a, 0x00, 0x00, 0x00, /* size of central directory = 90 */
	0x29, 0x00, 0x00, 0x00, /* offset of central directory = 41 */
	0x00, 0x00,             /* .ZIP file comment length */
};

DEFINE_TEST(test_read_format_zip_dos_mtime_precedence_ntfs)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_dos_mtime_precedence_ntfs,
		sizeof(archive_dos_mtime_precedence_ntfs)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualString("test.txt", archive_entry_pathname(ae));
	assertEqualInt(1700000000LL, archive_entry_mtime(ae));
	assertEqualInt(100000000L, archive_entry_mtime_nsec(ae));
	assertEqualInt(1700000100LL, archive_entry_atime(ae));
	assertEqualInt(200000000L, archive_entry_atime_nsec(ae));
	assert(archive_entry_birthtime_is_set(ae));
	assertEqualInt(1700000200LL, archive_entry_birthtime(ae));
	assertEqualInt(300000000L, archive_entry_birthtime_nsec(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* 0x000D "PKWARE Unix", Central Directory only. */
static const unsigned char archive_dos_mtime_precedence_pkware_unix[] = {
/* --- local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time (wrong-on-purpose: DOS_MIN_TIME) */
	0x21, 0x00,             /* last mod file date (wrong-on-purpose: DOS_MIN_TIME) */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x00, 0x00,             /* extra field length = 0 (no extra field here) */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x000d "PKWARE Unix" placed in the Central Directory ONLY -
 * the Local Header above has none: this is the scenario the
 * DOS-clobber guard exists for. */
	0x68, 0x69, 0x0a,       /* "hi\n" */
/* --- central directory header --- */
	0x50, 0x4b, 0x01, 0x02, /* central directory header signature */
	0x14, 0x00,             /* version made by: 2.0, MS-DOS */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time (unused: 0x000d "PKWARE Unix" wins) */
	0x21, 0x00,             /* last mod file date (unused: 0x000d "PKWARE Unix" wins) */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x10, 0x00,             /* extra field length = 16 */
	0x00, 0x00,             /* file comment length */
	0x00, 0x00,             /* disk number start */
	0x00, 0x00,             /* internal file attributes */
	0x00, 0x00, 0xa4, 0x81, /* external file attributes: -rw-r--r-- */
	0x00, 0x00, 0x00, 0x00, /* relative offset of local header = 0 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x000d "PKWARE Unix" (only copy: must set mtime) */
	0x0d, 0x00,             /* extra field ID = 0x000d */
	0x0c, 0x00,             /* extra field size = 12 */
	0x64, 0xf1, 0x53, 0x65, /* atime -> 1700000100 */
	0x00, 0xf1, 0x53, 0x65, /* mtime -> 1700000000 */
	0xe9, 0x03,             /* uid = 1001 */
	0xea, 0x03,             /* gid = 1002 */
/* --- end of central directory record --- */
	0x50, 0x4b, 0x05, 0x06, /* end of central directory signature */
	0x00, 0x00,             /* number of this disk */
	0x00, 0x00,             /* disk with start of central directory */
	0x01, 0x00,             /* central directory entries on this disk */
	0x01, 0x00,             /* total central directory entries */
	0x46, 0x00, 0x00, 0x00, /* size of central directory = 70 */
	0x29, 0x00, 0x00, 0x00, /* offset of central directory = 41 */
	0x00, 0x00,             /* .ZIP file comment length */
};

DEFINE_TEST(test_read_format_zip_dos_mtime_precedence_pkware_unix)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_dos_mtime_precedence_pkware_unix,
		sizeof(archive_dos_mtime_precedence_pkware_unix)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualString("test.txt", archive_entry_pathname(ae));
	assertEqualInt(1700000000LL, archive_entry_mtime(ae));
	assertEqualInt(1700000100LL, archive_entry_atime(ae));
	assertEqualInt(1001, archive_entry_uid(ae));
	assertEqualInt(1002, archive_entry_gid(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* 0x5455 "UT", Central Directory only. */
static const unsigned char archive_dos_mtime_precedence_ut[] = {
/* --- local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time (wrong-on-purpose: DOS_MIN_TIME) */
	0x21, 0x00,             /* last mod file date (wrong-on-purpose: DOS_MIN_TIME) */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x00, 0x00,             /* extra field length = 0 (no extra field here) */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x5455 "UT" placed in the Central Directory ONLY -
 * the Local Header above has none: this is the scenario the
 * DOS-clobber guard exists for. */
	0x68, 0x69, 0x0a,       /* "hi\n" */
/* --- central directory header --- */
	0x50, 0x4b, 0x01, 0x02, /* central directory header signature */
	0x14, 0x00,             /* version made by: 2.0, MS-DOS */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time (unused: 0x5455 "UT" wins) */
	0x21, 0x00,             /* last mod file date (unused: 0x5455 "UT" wins) */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x11, 0x00,             /* extra field length = 17 */
	0x00, 0x00,             /* file comment length */
	0x00, 0x00,             /* disk number start */
	0x00, 0x00,             /* internal file attributes */
	0x00, 0x00, 0xa4, 0x81, /* external file attributes: -rw-r--r-- */
	0x00, 0x00, 0x00, 0x00, /* relative offset of local header = 0 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x5455 "UT" (only copy: must set mtime) */
	0x55, 0x54,             /* extra field ID = 0x5455 */
	0x0d, 0x00,             /* extra field size = 13 */
	0x07,                   /* flags: mtime, atime, ctime all present */
	0x00, 0xf1, 0x53, 0x65, /* mtime -> 1700000000 */
	0x64, 0xf1, 0x53, 0x65, /* atime -> 1700000100 */
	0xc8, 0xf1, 0x53, 0x65, /* ctime -> 1700000200 */
/* --- end of central directory record --- */
	0x50, 0x4b, 0x05, 0x06, /* end of central directory signature */
	0x00, 0x00,             /* number of this disk */
	0x00, 0x00,             /* disk with start of central directory */
	0x01, 0x00,             /* central directory entries on this disk */
	0x01, 0x00,             /* total central directory entries */
	0x47, 0x00, 0x00, 0x00, /* size of central directory = 71 */
	0x29, 0x00, 0x00, 0x00, /* offset of central directory = 41 */
	0x00, 0x00,             /* .ZIP file comment length */
};

DEFINE_TEST(test_read_format_zip_dos_mtime_precedence_ut)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_dos_mtime_precedence_ut,
		sizeof(archive_dos_mtime_precedence_ut)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualString("test.txt", archive_entry_pathname(ae));
	assertEqualInt(1700000000LL, archive_entry_mtime(ae));
	assertEqualInt(1700000100LL, archive_entry_atime(ae));
	assertEqualInt(1700000200LL, archive_entry_ctime(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* 0x5855 "UX" (old), Central Directory only. */
static const unsigned char archive_dos_mtime_precedence_ux[] = {
/* --- local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time (wrong-on-purpose: DOS_MIN_TIME) */
	0x21, 0x00,             /* last mod file date (wrong-on-purpose: DOS_MIN_TIME) */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x00, 0x00,             /* extra field length = 0 (no extra field here) */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x5855 "UX" (old) placed in the Central Directory ONLY -
 * the Local Header above has none: this is the scenario the
 * DOS-clobber guard exists for. */
	0x68, 0x69, 0x0a,       /* "hi\n" */
/* --- central directory header --- */
	0x50, 0x4b, 0x01, 0x02, /* central directory header signature */
	0x14, 0x00,             /* version made by: 2.0, MS-DOS */
	0x14, 0x00,             /* version needed to extract: 2.0 */
	0x00, 0x00,             /* general purpose bit flag */
	0x00, 0x00,             /* compression method: stored */
	0x00, 0x00,             /* last mod file time (unused: 0x5855 "UX" (old) wins) */
	0x21, 0x00,             /* last mod file date (unused: 0x5855 "UX" (old) wins) */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,             /* file name length = 8 */
	0x10, 0x00,             /* extra field length = 16 */
	0x00, 0x00,             /* file comment length */
	0x00, 0x00,             /* disk number start */
	0x00, 0x00,             /* internal file attributes */
	0x00, 0x00, 0xa4, 0x81, /* external file attributes: -rw-r--r-- */
	0x00, 0x00, 0x00, 0x00, /* relative offset of local header = 0 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x5855 "UX" (old) (only copy: must set mtime) */
	0x55, 0x58,             /* extra field ID = 0x5855 */
	0x0c, 0x00,             /* extra field size = 12 */
	0x64, 0xf1, 0x53, 0x65, /* atime -> 1700000100 */
	0x00, 0xf1, 0x53, 0x65, /* mtime -> 1700000000 */
	0xe9, 0x03,             /* uid = 1001 */
	0xea, 0x03,             /* gid = 1002 */
/* --- end of central directory record --- */
	0x50, 0x4b, 0x05, 0x06, /* end of central directory signature */
	0x00, 0x00,             /* number of this disk */
	0x00, 0x00,             /* disk with start of central directory */
	0x01, 0x00,             /* central directory entries on this disk */
	0x01, 0x00,             /* total central directory entries */
	0x46, 0x00, 0x00, 0x00, /* size of central directory = 70 */
	0x29, 0x00, 0x00, 0x00, /* offset of central directory = 41 */
	0x00, 0x00,             /* .ZIP file comment length */
};

DEFINE_TEST(test_read_format_zip_dos_mtime_precedence_ux)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_dos_mtime_precedence_ux,
		sizeof(archive_dos_mtime_precedence_ux)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualString("test.txt", archive_entry_pathname(ae));
	assertEqualInt(1700000000LL, archive_entry_mtime(ae));
	assertEqualInt(1700000100LL, archive_entry_atime(ae));
	assertEqualInt(1001, archive_entry_uid(ae));
	assertEqualInt(1002, archive_entry_gid(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
