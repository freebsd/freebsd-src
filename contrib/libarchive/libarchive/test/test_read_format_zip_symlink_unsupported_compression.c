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
#include <fcntl.h>

/*
 * A symlink entry with an unrecognized compression format should FAIL
 * (that individual entry is unsupported) without preventing the rest of
 * the archive from being read.
 *
 * The Zip reader previously FAILED the entry but the archive read core
 * translated that to FATAL, preventing the client from continuing.  Updates
 * to the read core allow us to finally handle that correctly.
 */

static struct archive *
open_fixture(const char *refname)
{
	struct archive *a;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 4096));
	return a;
}

/* A well-behaved client that never tries to read data from the failed
 * entry must be able to recover and read the rest of the archive. */
static void
test_recovery(const char *refname)
{
	struct archive *a = open_fixture(refname);
	struct archive_entry *ae;
	char buff[32];

	failure("a format's ARCHIVE_FAILED from read_header must propagate, "
	    "not be silently swallowed or crash");
	assertEqualIntA(a, ARCHIVE_FAILED, archive_read_next_header(a, &ae));

	failure("archive_read_next_header() must still be able to advance "
	    "past a failed entry to the next one");
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("second", archive_entry_pathname(ae));
	assertEqualInt(11, (int)archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem(buff, "hello world", 11);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* A client that instead tries to read real content from the failed entry
 * must be refused (not handed garbage, not crash) -- and, matching every
 * other API-misuse case in libarchive, the archive object is left
 * unusable afterward rather than silently continuing. */
static void
test_illegal_read_data(const char *refname)
{
	struct archive *a = open_fixture(refname);
	struct archive_entry *ae;
	char buff[32];

	assertEqualIntA(a, ARCHIVE_FAILED, archive_read_next_header(a, &ae));

	failure("archive_read_data() must refuse content from a failed "
	    "entry, not return real (or garbage) bytes");
	assert(archive_read_data(a, buff, sizeof(buff)) < 0);

	failure("the archive must stay unusable after that misuse, not "
	    "silently recover");
	assert(archive_read_next_header(a, &ae) < 0);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_illegal_read_data_block(const char *refname)
{
	struct archive *a = open_fixture(refname);
	struct archive_entry *ae;
	const void *buff;
	size_t size;
	int64_t offset;

	assertEqualIntA(a, ARCHIVE_FAILED, archive_read_next_header(a, &ae));
	failure("archive_read_data_block() must refuse a failed entry");
	assert(archive_read_data_block(a, &buff, &size, &offset) < 0);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_illegal_seek_data(const char *refname)
{
	struct archive *a = open_fixture(refname);
	struct archive_entry *ae;

	assertEqualIntA(a, ARCHIVE_FAILED, archive_read_next_header(a, &ae));
	failure("archive_seek_data() must refuse a failed entry");
	assert(archive_seek_data(a, 0, SEEK_SET) < 0);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static void
test_illegal_read_data_into_fd(const char *refname)
{
	struct archive *a = open_fixture(refname);
	struct archive_entry *ae;
	int fd;

	assertEqualIntA(a, ARCHIVE_FAILED, archive_read_next_header(a, &ae));
	fd = open("data_into_fd.out", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	assert(fd >= 0);
	failure("archive_read_data_into_fd() must refuse a failed entry");
	assert(archive_read_data_into_fd(a, fd) < 0);
	close(fd);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/* Explicitly calling archive_read_data_skip() (rather than relying on the
 * implicit skip inside archive_read_next_header()) is a legal way to
 * recover, too. */
static void
test_explicit_skip(const char *refname)
{
	struct archive *a = open_fixture(refname);
	struct archive_entry *ae;

	assertEqualIntA(a, ARCHIVE_FAILED, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_skip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("second", archive_entry_pathname(ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_zip_symlink_unsupported_compression)
{
	const char *refname =
	    "test_read_format_zip_symlink_unsupported_compression.zip";

	test_recovery(refname);
	test_illegal_read_data(refname);
	test_illegal_read_data_block(refname);
	test_illegal_seek_data(refname);
	test_illegal_read_data_into_fd(refname);
	test_explicit_skip(refname);
}
