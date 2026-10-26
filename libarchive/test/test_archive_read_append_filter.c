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

#include "archive.h"
#include "test.h"

DEFINE_TEST(test_archive_read_append_filter)
{
	struct archive *a;
	int r;

	a = archive_read_new();
	assert(a != NULL);

	/* Append a program bidder (without filter). */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_filter_program(a, "prog1"));

	/* Append a filter program which comes with its own bidder. */
	r = archive_read_append_filter_program(a, "prog2");

	/* Verify that filter uses its own bidder. */
	if (r == ARCHIVE_FATAL) {
		assertEqualStringA(a, "Can't initialize filter; unable to run program \"prog2\"",
			archive_error_string(a));
	} else {
		assertEqualIntA(a, ARCHIVE_OK, r);
		assertEqualStringA(a, "Program: prog2", archive_filter_name(a, 0));
	}

	archive_read_free(a);
}
