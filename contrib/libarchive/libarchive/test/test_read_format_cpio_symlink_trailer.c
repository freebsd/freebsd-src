/*-
 * Copyright (c) 2026 Kaif Khan
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
 * The cpio reader reads an entry's name into a read-ahead buffer and holds
 * that pointer to compare the name against "TRAILER!!!" (end of archive).
 * For a symlink entry the link body is read first, and a large body can grow
 * (and free) the read-ahead buffer, leaving the name pointer dangling before
 * the trailer comparison.
 *
 * The reference archive is a single newc entry: a symlink named "TRAILER!!!"
 * with a 128 KiB body.  Reading it in small blocks assembles the name in the
 * reader's copy buffer, which the symlink body read then reallocates; the
 * trailer must still be detected without touching freed memory.
 */
DEFINE_TEST(test_read_format_cpio_symlink_trailer)
{
	const char *name = "test_read_format_cpio_symlink_trailer.cpio";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(name);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cpio(a));
	/* Small block size so the name is assembled in the reader's copy
	 * buffer, which the symlink body read then reallocates. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, name, 13));

	/* The "TRAILER!!!" entry ends the archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
