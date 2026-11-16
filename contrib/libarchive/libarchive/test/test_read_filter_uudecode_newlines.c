/*-
 * Copyright (c) 2026 Tobias Stoeckmann
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

#define __LIBARCHIVE_TEST
#include "archive_string.h"

static void
create_data(struct archive_string *s, const char *newline)
{
	int i;

	archive_string_sprintf(s, "begin 644 zeros%s", newline);
	for (i = 0; i < 18; i++)
		archive_string_sprintf(s, "M````````````````````````````````"
		    "````````````````````````````%s", newline);
	archive_string_sprintf(s, "`%s", newline);
	archive_string_sprintf(s, "end%s", newline);
}

static void
test_data(const char *newline)
{
	char buf[1024];
	struct archive *a;
	struct archive_entry *ae;
	struct archive_string s;

	archive_string_init(&s);
	create_data(&s, newline);

	/* Read 810 uu-encoded 0 bytes. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_uu(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory2(a, s.s, archive_strlen(&s), 4));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualIntA(a, 810, archive_read_data(a, &buf, sizeof(buf)));
	assertMemoryFilledWith(buf, 810, 0x00);
	assertEqualIntA(a, 0, archive_read_data(a, &buf, sizeof(buf)));

	/* Clean up resources. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	archive_string_free(&s);
}

DEFINE_TEST(test_read_filter_uudecode_newlines)
{
	test_data("\r");
	test_data("\n");
	test_data("\r\n");
}
