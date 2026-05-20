/*-
 * Copyright (c) 2003-2025 Tim Kientzle
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

#include <locale.h>

DEFINE_TEST(test_v7tar_filename_encoding_fail_UTF16_win)
{
#if !defined(_WIN32) || defined(__CYGWIN__)
	skipping("This test is meant to verify unicode string handling"
		" on Windows with UTF-16 names");
	return;
#else
	struct archive *a;
	struct archive_entry *entry;
	char buff[4096];
	size_t used;

	/* Test the C locale by not calling setlocale.  */

	a = archive_write_new();
	assertEqualInt(ARCHIVE_OK, archive_write_set_format_v7tar(a));
	if (archive_write_set_options(a, "hdrcharset=CP437") != ARCHIVE_OK) {
		skipping("This system cannot convert character-set"
		    " from UTF-16 to CP437.");
		archive_write_free(a);
		return;
	}
	assertEqualInt(ARCHIVE_OK,
	    archive_write_open_memory(a, buff, sizeof(buff), &used));

	entry = archive_entry_new2(a);
	/* Set the filename using a UTF-16 string */
	archive_entry_copy_pathname_w(entry, L"\u8868.txt");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 0);
	assertEqualInt(ARCHIVE_FAILED, archive_write_header(a, entry));
	/* The pathname cannot even be represented in the current locale
	   for inclusion in the error message.  */
	assertEqualString("Can't translate pathname to CP437",
	    archive_error_string(a));
	archive_entry_free(entry);
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
#endif
}
