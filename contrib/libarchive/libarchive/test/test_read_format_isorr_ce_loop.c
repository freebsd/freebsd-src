/*-
 * Copyright (c) 2026 Zhihan Zheng
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
 * A malformed Rock Ridge ISO whose first CE continuation area contains
 * another CE entry that points back to the same area. The parser should
 * reject this self-referential continuation chain with ARCHIVE_FATAL
 * instead of looping while reading headers.
 */

DEFINE_TEST(test_read_format_isorr_ce_loop)
{
	const char *refname = "test_read_format_iso_rockridge_ce_loop.iso.Z";
	struct archive *a;
	struct archive_entry *ae;
	int r;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 10240));

	/* Read entries until the self-referential CE is reached. */
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK)
		archive_read_data_skip(a);
	
	/* The parser should reject the self-referential CE with ARCHIVE_FATAL. */
	assertEqualIntA(a, ARCHIVE_FATAL, r);
	assert(archive_errno(a) != 0);
	assert(archive_error_string(a) != NULL);
	
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
