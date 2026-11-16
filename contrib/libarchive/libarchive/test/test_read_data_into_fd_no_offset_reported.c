/*-
 * Copyright (c) 2026 Kaixuan Li
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
 * archive_read_data_into_fd() must not depend on archive_read_data_block()
 * having written *offset.  A format's read_data callback may end an entry
 * on its very first call, and for an entry whose size is not set the
 * declared-size clamp does not apply either, so an offset the callback
 * never wrote reaches pad_to() and becomes a trailing hole of arbitrary
 * size.
 *
 * "hardlink.txt" in the reference archive is such an entry: rar5 ends it
 * on the first call, and archive_entry_size_is_set() is false for it.
 * Extracting it must produce an empty file, not a hole.
 *
 * Before the fix this failed below with "Seek error" (EINVAL), where
 * pad_to() lseek()s to the value the stack happened to hold.  That value
 * varies by compiler and version, so the visible symptom is not portable;
 * MemorySanitizer reports the read unconditionally.
 */
DEFINE_TEST(test_read_data_into_fd_no_offset_reported)
{
	const char *refname = "test_read_format_rar5_hardlink.rar";
	char tmpfilename[] = "no_offset_reported";
	struct archive *a;
	struct archive_entry *ae;
	int fd;
	struct stat st;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file.txt", archive_entry_pathname(ae));
	fd = open(tmpfilename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(fd >= 0);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, fd));
	close(fd);
	assertEqualInt(0, stat(tmpfilename, &st));
	assertEqualInt(5, (int)st.st_size);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("hardlink.txt", archive_entry_pathname(ae));
	assertEqualInt(0, archive_entry_size_is_set(ae));

	fd = open(tmpfilename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	assert(fd >= 0);
	failure("archive_read_data_into_fd() must succeed for an entry whose "
	    "reader reported no offset");
	assertEqualIntA(a, ARCHIVE_OK, archive_read_data_into_fd(a, fd));
	close(fd);

	assertEqualInt(0, stat(tmpfilename, &st));
	failure("an entry whose reader reported no offset must not produce a "
	    "trailing hole; got %d bytes", (int)st.st_size);
	assertEqualInt(0, (int)st.st_size);

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
