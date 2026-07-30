/*-
 * Copyright (c) 2026 Tim Kientzle
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

#if defined(_WIN32) && !defined(__CYGWIN__)
#define open _open
#define close _close
#endif

/*
 * archive_read_data_into_fd() writes archive_read_data_block() output
 * straight to an fd with no notion of the entry's declared size -- it's
 * used directly by callers like bsdtar's "-O" (stdout) extraction and
 * bsdcat, which bypass archive_write_disk (and the size-truncation
 * safeguard archive_write_disk already has).  This is a format-agnostic
 * backstop for that gap: it must never write more than the entry's
 * declared size, regardless of which format reader produced the data
 * or whether that reader independently enforces its own size boundary.
 *
 * The reference archives are crafted ZIP files (a stored entry whose
 * physical body is longer than its declared size, and a deflate entry
 * whose real inflated output exceeds 256KiB so it can't complete within
 * a single internal decode call -- see zip_read_data_deflate()'s
 * internal output buffer). Each has two entries: "first" declares
 * uncompressed_size=10 but its real content is longer, and "second" is
 * a well-formed 11-byte "hello world" file, read right after "first"
 * fails to confirm the mis-declared entry doesn't disrupt the rest of
 * the archive. This test is deliberately independent of the ZIP reader
 * itself enforcing this boundary -- archive_read_data_into_fd() must
 * cap its own output regardless of whether the format reader does.
 */
static void
verify(const char *refname)
{
	struct archive *a;
	struct archive_entry *ae;
	char tmpfilename[] = "data_into_fd_test";
	int fd;
	char buff[32];
	size_t n;
	FILE *f;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("first", archive_entry_pathname(ae));
	assertEqualInt(10, archive_entry_size(ae));

	fd = open(tmpfilename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(fd >= 0);
	failure("archive_read_data_into_fd() must report an error when the "
	    "entry's actual data exceeds its declared size");
	assert(archive_read_data_into_fd(a, fd) != ARCHIVE_OK);
	close(fd);

	f = fopen(tmpfilename, "rb");
	assert(f != NULL);
	n = fread(buff, 1, sizeof(buff), f);
	fclose(f);
	failure("archive_read_data_into_fd() must never write more bytes "
	    "than the entry's declared size");
	assert(n <= 10);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("second", archive_entry_pathname(ae));
	assertEqualInt(11, archive_entry_size(ae));

	fd = open(tmpfilename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(fd >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, fd));
	close(fd);

	f = fopen(tmpfilename, "rb");
	assert(f != NULL);
	n = fread(buff, 1, sizeof(buff), f);
	fclose(f);
	assertEqualInt(11, (int)n);
	assertEqualMem(buff, "hello world", 11);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_data_into_fd_size_exceeds_declared_stored)
{
	verify("test_read_data_into_fd_size_exceeds_declared_stored.zip");
}

DEFINE_TEST(test_read_data_into_fd_size_exceeds_declared_deflate)
{
	verify("test_read_data_into_fd_size_exceeds_declared_deflate.zip");
}
