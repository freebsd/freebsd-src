/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

/*
 * `bsdtar -x` writing to real files is protected by archive_write_disk,
 * which already truncates output at an entry's declared size.  `bsdtar
 * -xO` (stdout) bypasses archive_write_disk entirely and writes
 * archive_read_data_into_fd()'s output straight to fd 1 -- this test
 * exercises that path's own backstop (added directly to
 * archive_read_data_into_fd(), so it applies no matter which archive
 * format is being read) at the bsdtar CLI level.
 *
 * The reference archives are crafted ZIP files with two entries: "first"
 * declares uncompressed_size=10 but its true content is longer
 * ("AAAAAAAAAABBBBBBBBBB" for _stored, 256KiB+6 bytes of
 * "CCCCCCCCCCDDDDDDDDDD..." for _deflate), and "second" is a normal
 * 11-byte "hello world" file, extracted right after "first" fails to
 * confirm the mis-declared entry doesn't disrupt the rest of the
 * archive.
 *
 * The ZIP reader itself also rejects "first" outright (a separate,
 * format-level fix -- see test_read_format_zip_size_exceeds_declared),
 * so archive_read_data_block() never hands this backstop any bytes for
 * "first" to (partially) pass through: stdout ends up with none of
 * "first"'s data at all, followed directly by "second".  That's still
 * "never more than declared" -- just fewer bytes than the boundary
 * allows, since the format reader was already more conservative here.
 */
static void
verify(const char *refname)
{
	extract_reference_file(refname);
	failure("bsdtar -xO must report an error for the mis-declared entry");
	assert(systemf("%s -xOf %s >test.out 2>test.err", testprog, refname)
	    != 0);

	assertFileContents("hello world", 11, "test.out");
	assertNonEmptyFile("test.err");
}

DEFINE_TEST(test_option_stdout_size_exceeds_declared)
{
	verify("test_option_stdout_size_exceeds_declared_stored.zip");
	verify("test_option_stdout_size_exceeds_declared_deflate.zip");
}
