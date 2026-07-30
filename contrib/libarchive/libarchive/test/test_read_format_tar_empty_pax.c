/*-
 * Copyright (c) 2013 Tim Kientzle
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

#define TAR_BLOCK_SIZE 512

static void
format_octal(char *p, size_t s, unsigned long long v)
{
	memset(p, 0, s);
	snprintf(p, s, "%0*llo", (int)s - 1, v);
}

static void
build_tar_header(char *p, const char *name, char typeflag,
    unsigned long long size)
{
	unsigned int checksum;
	size_t i;

	memset(p, 0, TAR_BLOCK_SIZE);
	snprintf(p, 100, "%s", name);
	format_octal(p + 100, 8, 0644);
	format_octal(p + 108, 8, 0);
	format_octal(p + 116, 8, 0);
	format_octal(p + 124, 12, size);
	format_octal(p + 136, 12, 0);
	memset(p + 148, ' ', 8);
	p[156] = typeflag;
	memcpy(p + 257, "ustar", 5);
	memcpy(p + 263, "00", 2);

	checksum = 0;
	for (i = 0; i < TAR_BLOCK_SIZE; ++i)
		checksum += (unsigned char)p[i];
	snprintf(p + 148, 8, "%06o", checksum);
	p[154] = '\0';
	p[155] = ' ';
}

static void
build_empty_pax_archive(char *buff, char typeflag)
{
	memset(buff, 0, TAR_BLOCK_SIZE * 4);
	build_tar_header(buff,
	    typeflag == 'g' ? "pax_global_header" : "./PaxHeaders/file",
	    typeflag, 0);
	build_tar_header(buff + TAR_BLOCK_SIZE, "file", '0', 0);
}

static void
assert_empty_pax_rejected(char typeflag, const char *error)
{
	char archive[TAR_BLOCK_SIZE * 4];
	struct archive_entry *ae;
	struct archive *a;

	build_empty_pax_archive(archive, typeflag);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, archive, sizeof(archive)));
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualString(error, archive_error_string(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * A "usual" empty tar archive contains only zero bytes
 * and gets handled by the 'empty' format, not by the 'tar'
 * format.  But there are other kinds of empty tar archives
 * that are true tar archives and handled as such.
 */
DEFINE_TEST(test_read_format_tar_empty_pax)
{
	/*
	 * An archive that only contains a PAX 'g' record
	 * and no real files.  (Git will generate these when
	 * archiving an empty project.)
	 */
	struct archive_entry *ae;
	struct archive *a;
	const char *refname = "test_read_format_tar_empty_pax.tar.Z";

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualInt(ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualInt(ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(archive_entry_is_encrypted(ae), 0);
	assertEqualIntA(a, archive_read_has_encrypted_entries(a), ARCHIVE_READ_FORMAT_ENCRYPTION_UNSUPPORTED);
	assertEqualInt(ARCHIVE_FILTER_COMPRESS, archive_filter_code(a, 0));
	assertEqualInt(ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE, archive_format(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Empty pax extension headers are invalid: they contain no records. */
	assert_empty_pax_rejected('x', "Invalid empty pax extended header");
	assert_empty_pax_rejected('g', "Invalid empty pax global extended header");
}
