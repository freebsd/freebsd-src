/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
__FBSDID("$FreeBSD$");

#include <locale.h>

/*
 * Pax interchange is supposed to encode filenames into
 * UTF-8.  Of course, that's not always possible.  This
 * test is intended to verify that filenames always get
 * stored and restored correctly, regardless of the encodings.
 */

DEFINE_TEST(test_pax_filename_encoding)
{
	static const char testname[] = "test_pax_filename_encoding.tar.gz";
	char buff[65536];
	/*
	 * \314\214 is a valid 2-byte UTF-8 sequence.
	 * \374 is invalid in UTF-8.
	 */
	char filename[] = "abc\314\214mno\374xyz";
	char longname[] = "abc\314\214mno\374xyz"
	    "/abc\314\214mno\374xyz/abcdefghijklmnopqrstuvwxyz"
	    "/abc\314\214mno\374xyz/abcdefghijklmnopqrstuvwxyz"
	    "/abc\314\214mno\374xyz/abcdefghijklmnopqrstuvwxyz"
	    "/abc\314\214mno\374xyz/abcdefghijklmnopqrstuvwxyz"
	    "/abc\314\214mno\374xyz/abcdefghijklmnopqrstuvwxyz"
	    "/abc\314\214mno\374xyz/abcdefghijklmnopqrstuvwxyz"
	    ;
	size_t used;
	struct archive *a;
	struct archive_entry *entry;

	/*
	 * Read an archive that has non-UTF8 pax filenames in it.
	 */
	extract_reference_file(testname);
	a = archive_read_new();
	assertEqualInt(ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualInt(ARCHIVE_OK, archive_read_support_compression_gzip(a));
	assertEqualInt(ARCHIVE_OK,
	    archive_read_open_filename(a, testname, 10240));
	/*
	 * First entry in this test archive has an invalid UTF-8 sequence
	 * in it, but the header is not marked as hdrcharset=BINARY, so that
	 * requires a warning.
	 */
	failure("An invalid UTF8 pathname in a pax archive should be read\n"
	    " without conversion but with a warning");
	assertEqualInt(ARCHIVE_WARN, archive_read_next_header(a, &entry));
	assertEqualString(filename, archive_entry_pathname(entry));
	/*
	 * Second entry is identical except that it does have
	 * hdrcharset=BINARY, so no warning should be generated.
	 */
	failure("A pathname with hdrcharset=BINARY can have invalid UTF8\n"
	    " characters in it without generating a warning");
	assertEqualInt(ARCHIVE_OK, archive_read_next_header(a, &entry));
	assertEqualString(filename, archive_entry_pathname(entry));
	archive_read_finish(a);

	/*
	 * We need a starting locale which has invalid sequences.
	 * de_DE.UTF-8 seems to be commonly supported.
	 */
	/* If it doesn't exist, just warn and return. */
	failure("We need a suitable locale for the encoding tests.");
	if (!assert(NULL != setlocale(LC_ALL, "de_DE.UTF-8")))
		return;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, 0, archive_write_set_format_pax(a));
	assertEqualIntA(a, 0, archive_write_set_compression_none(a));
	assertEqualIntA(a, 0, archive_write_set_bytes_per_block(a, 0));
	assertEqualInt(0,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	assert((entry = archive_entry_new()) != NULL);
	/* Set pathname, gname, uname, hardlink to nonconvertible values. */
	archive_entry_copy_pathname(entry, filename);
	archive_entry_copy_gname(entry, filename);
	archive_entry_copy_uname(entry, filename);
	archive_entry_copy_hardlink(entry, filename);
	archive_entry_set_filetype(entry, AE_IFREG);
	failure("This should generate a warning for nonconvertible names.");
	assertEqualInt(ARCHIVE_WARN, archive_write_header(a, entry));
	archive_entry_free(entry);

	assert((entry = archive_entry_new()) != NULL);
	/* Set path, gname, uname, and symlink to nonconvertible values. */
	archive_entry_copy_pathname(entry, filename);
	archive_entry_copy_gname(entry, filename);
	archive_entry_copy_uname(entry, filename);
	archive_entry_copy_symlink(entry, filename);
	archive_entry_set_filetype(entry, AE_IFLNK);
	failure("This should generate a warning for nonconvertible names.");
	assertEqualInt(ARCHIVE_WARN, archive_write_header(a, entry));
	archive_entry_free(entry);

	assert((entry = archive_entry_new()) != NULL);
	/* Set pathname to a very long nonconvertible value. */
	archive_entry_copy_pathname(entry, longname);
	archive_entry_set_filetype(entry, AE_IFREG);
	failure("This should generate a warning for nonconvertible names.");
	assertEqualInt(ARCHIVE_WARN, archive_write_header(a, entry));
	archive_entry_free(entry);

	assertEqualInt(0, archive_write_close(a));
	assertEqualInt(0, archive_write_finish(a));

	/*
	 * Now read the entries back.
	 */

	assert((a = archive_read_new()) != NULL);
	assertEqualInt(0, archive_read_support_format_tar(a));
	assertEqualInt(0, archive_read_open_memory(a, buff, used));

	assertEqualInt(0, archive_read_next_header(a, &entry));
	assertEqualString(filename, archive_entry_pathname(entry));
	assertEqualString(filename, archive_entry_gname(entry));
	assertEqualString(filename, archive_entry_uname(entry));
	assertEqualString(filename, archive_entry_hardlink(entry));

	assertEqualInt(0, archive_read_next_header(a, &entry));
	assertEqualString(filename, archive_entry_pathname(entry));
	assertEqualString(filename, archive_entry_gname(entry));
	assertEqualString(filename, archive_entry_uname(entry));
	assertEqualString(filename, archive_entry_symlink(entry));

	assertEqualInt(0, archive_read_next_header(a, &entry));
	assertEqualString(longname, archive_entry_pathname(entry));

	assertEqualInt(0, archive_read_close(a));
	assertEqualInt(0, archive_read_finish(a));
}

