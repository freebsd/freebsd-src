/*-
 * Copyright (c) 2019 Martin Matuska
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
__FBSDID("$FreeBSD");

#include <locale.h>

static void
test_read_format_lha_filename_UTF16_UTF8(const char *refname)
{
	struct archive *a;
	struct archive_entry *ae;

	/*
	 * Read LHA filename in en_US.UTF-8.
	 */
	if (NULL == setlocale(LC_ALL, "en_US.UTF-8")) {
		skipping("en_US.UTF-8 locale not available on this system.");
		return;
	}
	/*
	 * Create a read object only for a test that platform support
	 * a character-set conversion because we can read a character-set
	 * of filenames from the header of an lha archive file and so we
	 * want to test that it works well. 
	 */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
    if (ARCHIVE_OK != archive_read_set_options(a, "hdrcharset=CP932")) {
        assertEqualInt(ARCHIVE_OK, archive_read_free(a));
        skipping("This system cannot convert character-set"
            " from CP932 to UTF-8.");
        return;
    }
	if (ARCHIVE_OK != archive_read_set_options(a, "hdrcharset=UTF-16")) {
		assertEqualInt(ARCHIVE_OK, archive_read_free(a));
		skipping("This system cannot convert character-set"
		    " from UTF-16 to UTF-8.");
		return;
	}
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Note that usual Japanese filenames are tested in other cases */
#if defined(__APPLE__)
 /* NFD normalization */
 /* U:O:A:u:o:a: */
 #define UMLAUT_DIRNAME "\x55\xcc\x88\x4f\xcc\x88\x41\xcc\x88\x75\xcc\x88\x6f"\
	    "\xcc\x88\x61\xcc\x88/"
 /* a:o:u:A:O:U:.txt */
 #define UMLAUT_FNAME "\x61\xcc\x88\x6f\xcc\x88\x75\xcc\x88\x41\xcc\x88"\
	    "\x4f\xcc\x88\x55\xcc\x88.txt"
#else
 /* NFC normalization */
 /* U:O:A:u:o:a: */
 #define UMLAUT_DIRNAME "\xc3\x9c\xc3\x96\xc3\x84\xc3\xbc\xc3\xb6\xc3\xa4/"
 /* a:o:u:A:O:U:.txt */
 #define UMLAUT_FNAME "\xc3\xa4\xc3\xb6\xc3\xbc\xc3\x84\xc3\x96\xc3\x9c.txt"
#endif

/* "Test" in Japanese Katakana */
#define KATAKANA_FNAME "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88.txt"
#define KATAKANA_DIRNAME "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88/"

	/* Verify regular file. U:O:A:u:o:a:/a:o:u:A:O:U:.txt */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(UMLAUT_DIRNAME UMLAUT_FNAME, archive_entry_pathname(ae));
	assertEqualInt(12, archive_entry_size(ae));

	/* Verify directory. U:O:A:u:o:a:/ */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(UMLAUT_DIRNAME, archive_entry_pathname(ae));
	assertEqualInt(0, archive_entry_size(ae));

	/* Verify regular file. U:O:A:u:o:a:/("Test" in Japanese).txt */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(UMLAUT_DIRNAME KATAKANA_FNAME,
	    archive_entry_pathname(ae));
	assertEqualInt(25, archive_entry_size(ae));

	/* Verify regular file. ("Test" in Japanese)/a:o:u:A:O:U:.txt */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(KATAKANA_DIRNAME UMLAUT_FNAME,
	    archive_entry_pathname(ae));
	assertEqualInt(12, archive_entry_size(ae));

	/* Verify directory. ("Test" in Japanese)/ */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(KATAKANA_DIRNAME, archive_entry_pathname(ae));
	assertEqualInt(0, archive_entry_size(ae));

	/* Verify regular file. a:o:u:A:O:U:.txt */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(UMLAUT_FNAME, archive_entry_pathname(ae));
	assertEqualInt(12, archive_entry_size(ae));

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_NONE, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_LHA, archive_format(a));

	/* Close the archive. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_read_format_lha_filename_UTF16)
{
	/* A sample file was created with Unlha32.dll. */
	const char *refname = "test_read_format_lha_filename_utf16.lzh";
	extract_reference_file(refname);

	test_read_format_lha_filename_UTF16_UTF8(refname);
}

