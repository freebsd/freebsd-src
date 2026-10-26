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
 * Regression tests for the disambiguation of a raw 32-bit ZIP timestamp
 * whose high bit is set, in the three extra fields that carry them:
 * 0x000D (PKWARE Unix), 0x5455 (Info-Zip "UT" extended timestamp), and
 * 0x5855 (Info-Zip "UX", old version).
 *
 * Such a value is ambiguous: read as signed, it is a pre-1970 date (as
 * far back as 1901-12-13); read as unsigned, it is a post-2038 date (as
 * far forward as 2106-02-07). Per
 * https://github.com/libarchive/libarchive/issues/3550, the signed
 * reading is preferred unless it would land before 1960 - no real file
 * should predate computing itself by that much - in which case the
 * unsigned reading is used instead.
 *
 * Each archive below has a single entry ("test.txt") probing this cutoff
 * from both sides, plus the canonical halfway value 0x80000000 (which
 * used to always resolve to 1901-12-13, the oldest date the old,
 * sign-extension-only code could produce).
 */

/* 0x000D "PKWARE Unix Extra Field": AcTime, ModTime, UID, GID.
 * atime = 0xed30087f (one second before the 1960 cutoff -> unsigned
 * reading, 2096-02-06 06:28:15 UTC).
 * mtime = 0x80000000 (the halfway value -> unsigned reading,
 * 2038-01-19 03:14:08 UTC, not the old 1901-12-13). */
static const unsigned char archive_pkware_unix_ambiguous[] = {
/* --- archive_pkware_unix_ambiguous: local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,          /* version needed to extract: 2.0 */
	0x00, 0x00,          /* general purpose bit flag */
	0x00, 0x00,          /* compression method: stored */
	0x00, 0x00,          /* last mod file time */
	0x21, 0x00,          /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,          /* file name length = 8 */
	0x10, 0x00,          /* extra field length = 16 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x000d */
	0x0d, 0x00,          /* extra field ID = 0x000d */
	0x0c, 0x00,          /* extra field size = 12 */
	0x7f, 0x08, 0x30, 0xed, /* atime -> 3979348095 (unsigned) */
	0x00, 0x00, 0x00, 0x80, /* mtime -> 2147483648 (unsigned) */
	0xe9, 0x03,          /* uid = 1001 */
	0xea, 0x03,          /* gid = 1002 */
	0x68, 0x69, 0x0a,    /* "hi\n" */
/* --- archive_pkware_unix_ambiguous: central directory header --- */
	0x50, 0x4b, 0x01, 0x02, /* central directory header signature */
	0x14, 0x00,          /* version made by: 2.0, MS-DOS */
	0x14, 0x00,          /* version needed to extract: 2.0 */
	0x00, 0x00,          /* general purpose bit flag */
	0x00, 0x00,          /* compression method: stored */
	0x00, 0x00,          /* last mod file time */
	0x21, 0x00,          /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,          /* file name length = 8 */
	0x10, 0x00,          /* extra field length = 16 */
	0x00, 0x00,          /* file comment length */
	0x00, 0x00,          /* disk number start */
	0x00, 0x00,          /* internal file attributes */
	0x00, 0x00, 0xa4, 0x81, /* external file attributes: -rw-r--r-- */
	0x00, 0x00, 0x00, 0x00, /* relative offset of local header = 0 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x000d */
	0x0d, 0x00,          /* extra field ID = 0x000d */
	0x0c, 0x00,          /* extra field size = 12 */
	0x7f, 0x08, 0x30, 0xed, /* atime -> 3979348095 (unsigned) */
	0x00, 0x00, 0x00, 0x80, /* mtime -> 2147483648 (unsigned) */
	0xe9, 0x03,          /* uid = 1001 */
	0xea, 0x03,          /* gid = 1002 */
/* --- archive_pkware_unix_ambiguous: end of central directory record --- */
	0x50, 0x4b, 0x05, 0x06, /* end of central directory signature */
	0x00, 0x00,          /* number of this disk */
	0x00, 0x00,          /* disk with start of central directory */
	0x01, 0x00,          /* central directory entries on this disk */
	0x01, 0x00,          /* total central directory entries */
	0x46, 0x00, 0x00, 0x00, /* size of central directory = 70 */
	0x39, 0x00, 0x00, 0x00, /* offset of central directory = 57 */
	0x00, 0x00,          /* .ZIP file comment length */
};

DEFINE_TEST(test_read_format_zip_ambiguous_timestamp_pkware_unix)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_pkware_unix_ambiguous,
		sizeof(archive_pkware_unix_ambiguous)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualString("test.txt", archive_entry_pathname(ae));
	if (sizeof(time_t) <= sizeof(int32_t)) {
		/* time_t this narrow can't hold a post-2038 date, so
		 * to_time_t() always sign-extends here instead of
		 * picking the unsigned reading; see the matching
		 * comment in archive_read_support_format_zip.c. */
		assertEqualInt(-2147483648LL, archive_entry_mtime(ae));
		assertEqualInt(-315619201LL, archive_entry_atime(ae));
	} else {
		assertEqualInt(2147483648LL, archive_entry_mtime(ae));
		assertEqualInt(3979348095LL, archive_entry_atime(ae));
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* 0x5455 "UT" extended timestamp: Flags, ModTime, AcTime, CrTime.
 * mtime = 0xed300880 (exactly the 1960 cutoff -> signed reading,
 * 1960-01-01 00:00:00 UTC).
 * atime = 0xed300881 (one second past the cutoff -> signed reading,
 * 1960-01-01 00:00:01 UTC).
 * ctime = 0xffffffff (-1 as signed -> 1969-12-31 23:59:59 UTC, an
 * ordinary near-1970 value, still handled correctly through the new
 * helper). */
static const unsigned char archive_ut_ambiguous[] = {
/* --- archive_ut_ambiguous: local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,          /* version needed to extract: 2.0 */
	0x00, 0x00,          /* general purpose bit flag */
	0x00, 0x00,          /* compression method: stored */
	0x00, 0x00,          /* last mod file time */
	0x21, 0x00,          /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,          /* file name length = 8 */
	0x11, 0x00,          /* extra field length = 17 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x5455 */
	0x55, 0x54,          /* extra field ID = 0x5455 */
	0x0d, 0x00,          /* extra field size = 13 */
	0x07,                /* flags: mtime, atime, ctime all present */
	0x80, 0x08, 0x30, 0xed, /* mtime -> -315619200 (signed) */
	0x81, 0x08, 0x30, 0xed, /* atime -> -315619199 (signed) */
	0xff, 0xff, 0xff, 0xff, /* ctime -> -1 (signed) */
	0x68, 0x69, 0x0a,    /* "hi\n" */
/* --- archive_ut_ambiguous: central directory header --- */
	0x50, 0x4b, 0x01, 0x02, /* central directory header signature */
	0x14, 0x00,          /* version made by: 2.0, MS-DOS */
	0x14, 0x00,          /* version needed to extract: 2.0 */
	0x00, 0x00,          /* general purpose bit flag */
	0x00, 0x00,          /* compression method: stored */
	0x00, 0x00,          /* last mod file time */
	0x21, 0x00,          /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,          /* file name length = 8 */
	0x11, 0x00,          /* extra field length = 17 */
	0x00, 0x00,          /* file comment length */
	0x00, 0x00,          /* disk number start */
	0x00, 0x00,          /* internal file attributes */
	0x00, 0x00, 0xa4, 0x81, /* external file attributes: -rw-r--r-- */
	0x00, 0x00, 0x00, 0x00, /* relative offset of local header = 0 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x5455 */
	0x55, 0x54,          /* extra field ID = 0x5455 */
	0x0d, 0x00,          /* extra field size = 13 */
	0x07,                /* flags: mtime, atime, ctime all present */
	0x80, 0x08, 0x30, 0xed, /* mtime -> -315619200 (signed) */
	0x81, 0x08, 0x30, 0xed, /* atime -> -315619199 (signed) */
	0xff, 0xff, 0xff, 0xff, /* ctime -> -1 (signed) */
/* --- archive_ut_ambiguous: end of central directory record --- */
	0x50, 0x4b, 0x05, 0x06, /* end of central directory signature */
	0x00, 0x00,          /* number of this disk */
	0x00, 0x00,          /* disk with start of central directory */
	0x01, 0x00,          /* central directory entries on this disk */
	0x01, 0x00,          /* total central directory entries */
	0x47, 0x00, 0x00, 0x00, /* size of central directory = 71 */
	0x3a, 0x00, 0x00, 0x00, /* offset of central directory = 58 */
	0x00, 0x00,          /* .ZIP file comment length */
};

DEFINE_TEST(test_read_format_zip_ambiguous_timestamp_ut)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_ut_ambiguous,
		sizeof(archive_ut_ambiguous)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualString("test.txt", archive_entry_pathname(ae));
	assertEqualInt(-315619200LL, archive_entry_mtime(ae));
	assertEqualInt(-315619199LL, archive_entry_atime(ae));
	assertEqualInt(-1LL, archive_entry_ctime(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* 0x5855 "UX" (old): AcTime, ModTime, UID, GID.
 * atime = 0xed300880 (exactly the 1960 cutoff -> signed reading,
 * 1960-01-01 00:00:00 UTC).
 * mtime = 0x80000000 (the halfway value -> unsigned reading,
 * 2038-01-19 03:14:08 UTC). */
static const unsigned char archive_ux_ambiguous[] = {
/* --- archive_ux_ambiguous: local file header --- */
	0x50, 0x4b, 0x03, 0x04, /* local file header signature */
	0x14, 0x00,          /* version needed to extract: 2.0 */
	0x00, 0x00,          /* general purpose bit flag */
	0x00, 0x00,          /* compression method: stored */
	0x00, 0x00,          /* last mod file time */
	0x21, 0x00,          /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,          /* file name length = 8 */
	0x10, 0x00,          /* extra field length = 16 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x5855 */
	0x55, 0x58,          /* extra field ID = 0x5855 */
	0x0c, 0x00,          /* extra field size = 12 */
	0x80, 0x08, 0x30, 0xed, /* atime -> -315619200 (signed) */
	0x00, 0x00, 0x00, 0x80, /* mtime -> 2147483648 (unsigned) */
	0xe9, 0x03,          /* uid = 1001 */
	0xea, 0x03,          /* gid = 1002 */
	0x68, 0x69, 0x0a,    /* "hi\n" */
/* --- archive_ux_ambiguous: central directory header --- */
	0x50, 0x4b, 0x01, 0x02, /* central directory header signature */
	0x14, 0x00,          /* version made by: 2.0, MS-DOS */
	0x14, 0x00,          /* version needed to extract: 2.0 */
	0x00, 0x00,          /* general purpose bit flag */
	0x00, 0x00,          /* compression method: stored */
	0x00, 0x00,          /* last mod file time */
	0x21, 0x00,          /* last mod file date */
	0x7a, 0x7a, 0x6f, 0xed, /* CRC-32 = 0xed6f7a7a */
	0x03, 0x00, 0x00, 0x00, /* compressed size = 3 */
	0x03, 0x00, 0x00, 0x00, /* uncompressed size = 3 */
	0x08, 0x00,          /* file name length = 8 */
	0x10, 0x00,          /* extra field length = 16 */
	0x00, 0x00,          /* file comment length */
	0x00, 0x00,          /* disk number start */
	0x00, 0x00,          /* internal file attributes */
	0x00, 0x00, 0xa4, 0x81, /* external file attributes: -rw-r--r-- */
	0x00, 0x00, 0x00, 0x00, /* relative offset of local header = 0 */
	0x74, 0x65, 0x73, 0x74, 0x2e, 0x74, 0x78, 0x74, /* "test.txt" */
/* extra field 0x5855 */
	0x55, 0x58,          /* extra field ID = 0x5855 */
	0x0c, 0x00,          /* extra field size = 12 */
	0x80, 0x08, 0x30, 0xed, /* atime -> -315619200 (signed) */
	0x00, 0x00, 0x00, 0x80, /* mtime -> 2147483648 (unsigned) */
	0xe9, 0x03,          /* uid = 1001 */
	0xea, 0x03,          /* gid = 1002 */
/* --- archive_ux_ambiguous: end of central directory record --- */
	0x50, 0x4b, 0x05, 0x06, /* end of central directory signature */
	0x00, 0x00,          /* number of this disk */
	0x00, 0x00,          /* disk with start of central directory */
	0x01, 0x00,          /* central directory entries on this disk */
	0x01, 0x00,          /* total central directory entries */
	0x46, 0x00, 0x00, 0x00, /* size of central directory = 70 */
	0x39, 0x00, 0x00, 0x00, /* offset of central directory = 57 */
	0x00, 0x00,          /* .ZIP file comment length */
};

DEFINE_TEST(test_read_format_zip_ambiguous_timestamp_ux)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_zip_seekable(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive_ux_ambiguous,
		sizeof(archive_ux_ambiguous)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	assertEqualString("test.txt", archive_entry_pathname(ae));
	if (sizeof(time_t) <= sizeof(int32_t)) {
		/* time_t this narrow can't hold a post-2038 date, so
		 * to_time_t() always sign-extends here instead of
		 * picking the unsigned reading; see the matching
		 * comment in archive_read_support_format_zip.c. */
		assertEqualInt(-2147483648LL, archive_entry_mtime(ae));
	} else {
		assertEqualInt(2147483648LL, archive_entry_mtime(ae));
	}
	assertEqualInt(-315619200LL, archive_entry_atime(ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
