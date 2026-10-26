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
 * A ZIP entry only has a modification time (in the legacy DOS fields) by
 * default. Its access time, creation time and so on come from optional
 * extra fields. When no extra field supplies atime, or ctime, the reader
 * must leave it unset rather than report it as set to the Unix epoch.
 *
 * Each test case builds a ZIP file with a single entry and the given extra
 * field data, and checks which timestamps of the entry are set. Every case
 * is run with both the seekable reader and the streaming reader.
 */

#define MTIME	1700000000LL
#define ATIME	1700000100LL
#define CTIME	1700000200LL

static void
put16(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)(v & 0xff);
	p[1] = (unsigned char)((v >> 8) & 0xff);
}

static void
put32(unsigned char *p, unsigned long v)
{
	put16(p, (unsigned)(v & 0xffff));
	put16(p + 2, (unsigned)((v >> 16) & 0xffff));
}

static void
put64(unsigned char *p, uint64_t v)
{
	put32(p, (unsigned long)(v & 0xffffffffUL));
	put32(p + 4, (unsigned long)((v >> 32) & 0xffffffffUL));
}

/* A Windows FILETIME, i.e. 100ns ticks since 1601-01-01. */
static uint64_t
filetime(long long secs, long nsecs)
{
	return ((uint64_t)secs + 11644473600ULL) * 10000000ULL + nsecs / 100;
}

/* 0x5455 "UT": flags tell which of mtime (1), atime (2), ctime (4) follow. */
static size_t
make_ut(unsigned char *p, int flags)
{
	size_t n = 5;

	put16(p, 0x5455);
	p[4] = (unsigned char)flags;
	if (flags & 1) {
		put32(p + n, MTIME);
		n += 4;
	}
	if (flags & 2) {
		put32(p + n, ATIME);
		n += 4;
	}
	if (flags & 4) {
		put32(p + n, CTIME);
		n += 4;
	}
	put16(p + 2, (unsigned)(n - 4));
	return (n);
}

/* 0x5855 "UX": atime and mtime. */
static size_t
make_ux(unsigned char *p)
{
	put16(p, 0x5855);
	put16(p + 2, 8);
	put32(p + 4, ATIME);
	put32(p + 8, MTIME);
	return (12);
}

/* 0x000D "PKWARE Unix": atime, mtime, uid, gid. */
static size_t
make_pkware_unix(unsigned char *p)
{
	put16(p, 0x000d);
	put16(p + 2, 12);
	put32(p + 4, ATIME);
	put32(p + 8, MTIME);
	put16(p + 12, 1000);
	put16(p + 14, 1000);
	return (16);
}

/* 0x000A "NTFS": a zero FILETIME stands for a time that isn't there. */
static size_t
make_ntfs(unsigned char *p, int has_mtime, int has_atime, int has_btime)
{
	put16(p, 0x000a);
	put16(p + 2, 32);
	put32(p + 4, 0);
	put16(p + 8, 1);
	put16(p + 10, 24);
	put64(p + 12, has_mtime ? filetime(MTIME, 100000000L) : 0);
	put64(p + 20, has_atime ? filetime(ATIME, 200000000L) : 0);
	put64(p + 28, has_btime ? filetime(CTIME, 300000000L) : 0);
	return (36);
}

/*
 * Builds a ZIP file made of a single stored entry named "test.txt". The
 * Local Header gets lh_extra, and the Central Directory gets cd_extra.
 * Returns the size of the ZIP file.
 */
static size_t
build_zip(unsigned char *zip, const unsigned char *lh_extra, size_t lh_len,
    const unsigned char *cd_extra, size_t cd_len)
{
	static const char name[] = "test.txt";
	static const char data[] = "hi\n";
	const size_t name_len = sizeof(name) - 1;
	const size_t data_len = sizeof(data) - 1;
	size_t n = 0, cd_offset, cd_size;

	/* Local file header. */
	put32(zip + n, 0x04034b50UL);
	put16(zip + n + 4, 20);		/* version needed to extract */
	put16(zip + n + 6, 0);		/* general purpose bit flag */
	put16(zip + n + 8, 0);		/* compression method: stored */
	put16(zip + n + 10, 0);		/* last mod file time */
	put16(zip + n + 12, 0x21);	/* last mod file date: 1980-01-01 */
	put32(zip + n + 14, 0xed6f7a7aUL);	/* CRC-32 of the data */
	put32(zip + n + 18, (unsigned long)data_len);
	put32(zip + n + 22, (unsigned long)data_len);
	put16(zip + n + 26, (unsigned)name_len);
	put16(zip + n + 28, (unsigned)lh_len);
	n += 30;
	memcpy(zip + n, name, name_len);
	n += name_len;
	memcpy(zip + n, lh_extra, lh_len);
	n += lh_len;
	memcpy(zip + n, data, data_len);
	n += data_len;

	/* Central directory header. */
	cd_offset = n;
	put32(zip + n, 0x02014b50UL);
	put16(zip + n + 4, 20);		/* version made by */
	put16(zip + n + 6, 20);		/* version needed to extract */
	put16(zip + n + 8, 0);		/* general purpose bit flag */
	put16(zip + n + 10, 0);		/* compression method: stored */
	put16(zip + n + 12, 0);		/* last mod file time */
	put16(zip + n + 14, 0x21);	/* last mod file date: 1980-01-01 */
	put32(zip + n + 16, 0xed6f7a7aUL);
	put32(zip + n + 20, (unsigned long)data_len);
	put32(zip + n + 24, (unsigned long)data_len);
	put16(zip + n + 28, (unsigned)name_len);
	put16(zip + n + 30, (unsigned)cd_len);
	put16(zip + n + 32, 0);		/* file comment length */
	put16(zip + n + 34, 0);		/* disk number start */
	put16(zip + n + 36, 0);		/* internal file attributes */
	put32(zip + n + 38, 0x81a40000UL);	/* -rw-r--r-- */
	put32(zip + n + 42, 0);		/* offset of local header */
	n += 46;
	memcpy(zip + n, name, name_len);
	n += name_len;
	memcpy(zip + n, cd_extra, cd_len);
	n += cd_len;
	cd_size = n - cd_offset;

	/* End of central directory record. */
	put32(zip + n, 0x06054b50UL);
	put16(zip + n + 4, 0);		/* number of this disk */
	put16(zip + n + 6, 0);		/* disk with the central directory */
	put16(zip + n + 8, 1);		/* entries on this disk */
	put16(zip + n + 10, 1);		/* total entries */
	put32(zip + n + 12, (unsigned long)cd_size);
	put32(zip + n + 16, (unsigned long)cd_offset);
	put16(zip + n + 20, 0);		/* comment length */
	n += 22;
	return (n);
}

/* Which timestamps of the entry are expected to be set, and their values. */
struct expected {
	int atime_set;
	long long atime;
	long atime_nsec;
	int ctime_set;
	long long ctime;
	int btime_set;
};

static void
check_entry(struct archive_entry *ae, const struct expected *e)
{
	/* The DOS time is always there. */
	assert(archive_entry_mtime_is_set(ae));

	assertEqualInt(e->atime_set, archive_entry_atime_is_set(ae) != 0);
	if (e->atime_set) {
		assertEqualInt(e->atime, archive_entry_atime(ae));
		assertEqualInt(e->atime_nsec, archive_entry_atime_nsec(ae));
	}

	assertEqualInt(e->ctime_set, archive_entry_ctime_is_set(ae) != 0);
	if (e->ctime_set)
		assertEqualInt(e->ctime, archive_entry_ctime(ae));

	assertEqualInt(e->btime_set, archive_entry_birthtime_is_set(ae) != 0);
}

/*
 * Reads the given ZIP file with the seekable reader, or with the streaming
 * reader, and checks the timestamps that are set on its only entry.
 */
static void
check_zip(const unsigned char *zip, size_t size, int seekable,
    const struct expected *e)
{
	struct archive *a;
	struct archive_entry *ae;

	assert((a = archive_read_new()) != NULL);
	if (seekable)
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_zip_seekable(a));
	else
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_read_support_format_zip_streamable(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, zip, size));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("test.txt", archive_entry_pathname(ae));
	check_entry(ae, e);
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* Runs a test case with both readers. */
static void
check_both_readers(const unsigned char *lh_extra, size_t lh_len,
    const unsigned char *cd_extra, size_t cd_len, const struct expected *e)
{
	unsigned char zip[512];
	size_t size;

	size = build_zip(zip, lh_extra, lh_len, cd_extra, cd_len);
	assert(size <= sizeof(zip));
	check_zip(zip, size, 1, e);
	check_zip(zip, size, 0, e);
}

DEFINE_TEST(test_read_format_zip_unset_times)
{
	unsigned char extra[64];
	size_t n;
	struct expected e;

	memset(extra, 0, sizeof(extra));

	/* No extra field at all: only mtime is set. */
	memset(&e, 0, sizeof(e));
	check_both_readers(extra, 0, extra, 0, &e);

	/* "UT" with mtime only. */
	n = make_ut(extra, 1);
	check_both_readers(extra, n, extra, n, &e);

	/* "UT" with mtime and atime. */
	n = make_ut(extra, 3);
	e.atime_set = 1;
	e.atime = ATIME;
	check_both_readers(extra, n, extra, n, &e);

	/* "UT" with mtime and ctime, but no atime. */
	n = make_ut(extra, 5);
	e.atime_set = 0;
	e.ctime_set = 1;
	e.ctime = CTIME;
	check_both_readers(extra, n, extra, n, &e);

	/* "UT" with all three times. */
	n = make_ut(extra, 7);
	e.atime_set = 1;
	e.atime = ATIME;
	check_both_readers(extra, n, extra, n, &e);

	/* "UX" supplies atime, but not ctime. */
	n = make_ux(extra);
	e.ctime_set = 0;
	check_both_readers(extra, n, extra, n, &e);

	/* "PKWARE Unix" supplies atime, but not ctime. */
	n = make_pkware_unix(extra);
	check_both_readers(extra, n, extra, n, &e);

	/* "NTFS" with a time in each of its three slots. */
	n = make_ntfs(extra, 1, 1, 1);
	e.atime_nsec = 200000000L;
	e.btime_set = 1;
	check_both_readers(extra, n, extra, n, &e);

	/* "NTFS" without atime: that slot is a zero FILETIME. */
	n = make_ntfs(extra, 1, 0, 1);
	e.atime_set = 0;
	check_both_readers(extra, n, extra, n, &e);

	/* "NTFS" with mtime only. */
	n = make_ntfs(extra, 1, 0, 0);
	e.btime_set = 0;
	check_both_readers(extra, n, extra, n, &e);
}

/*
 * The seekable reader reads the extra field of the Central Directory before
 * that of the Local Header. A time that only the Central Directory has is
 * still set, and a time that neither has remains unset.
 */
DEFINE_TEST(test_read_format_zip_unset_times_central_directory_only)
{
	unsigned char extra[64];
	unsigned char zip[512];
	size_t n, size;
	struct expected e;

	memset(&e, 0, sizeof(e));
	e.atime_set = 1;
	e.atime = ATIME;
	e.ctime_set = 1;
	e.ctime = CTIME;

	n = make_ut(extra, 7);
	size = build_zip(zip, extra, 0, extra, n);
	check_zip(zip, size, 1, &e);

	/* The streaming reader never sees the Central Directory. */
	memset(&e, 0, sizeof(e));
	check_zip(zip, size, 0, &e);
}
