/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2011 Michihiro NAKAJIMA
 * Copyright (c) 2019 Mike Frysinger
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

static void
verify(struct archive *a) {
	struct archive_entry *ae;
	const char *p;

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assert((p = archive_entry_pathname_utf8(ae)) != NULL);
	assertEqualUTF8String(p, "File 1.txt");

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assert((p = archive_entry_pathname_utf8(ae)) != NULL);
#if defined(__APPLE__)
	/* Compare NFD string. */
	assertEqualUTF8String(p, "File 2 - o\xCC\x88.txt");
#else
	/* Compare NFC string. */
	assertEqualUTF8String(p, "File 2 - \xC3\xB6.txt");
#endif

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assert((p = archive_entry_pathname_utf8(ae)) != NULL);
#if defined(__APPLE__)
	/* Compare NFD string. */
	assertEqualUTF8String(p, "File 3 - a\xCC\x88.txt");
#else
	/* Compare NFC string. */
	assertEqualUTF8String(p, "File 3 - \xC3\xA4.txt");
#endif

	/* The CRC of the filename fails, so fall back to CDE. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assert((p = archive_entry_pathname_utf8(ae)) != NULL);
	assertEqualUTF8String(p, "File 4 - xx.txt");

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
}

DEFINE_TEST(test_read_format_zip_utf8_paths)
{
	const char *refname = "test_read_format_zip_7075_utf8_paths.zip";
	struct archive *a;
	char *p;
	size_t s;

	extract_reference_file(refname);

	if (NULL == setlocale(LC_ALL, "en_US.UTF-8")) {
		skipping("en_US.UTF-8 locale not available on this system.");
		return;
	}

	/* Verify with seeking reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 10240));
	verify(a);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_free(a));

	/* Verify with streaming reader. */
	p = slurpfile(&s, "%s", refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory(a, p, s, 31));
	verify(a);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_free(a));
	free(p);
}
