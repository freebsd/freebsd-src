/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 iPuppeteer - Red Centipede Security
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
 * A malformed Rock Ridge ISO whose "CL" (child link) entries reparent
 * directories into a cycle of "parent" pointers in which no node carries
 * the "RE" (relocated entry) flag.  rede_add_entry() walks that parent
 * chain looking for the "RE" ancestor; before the fix the walk had no
 * termination guard, so the cyclic chain spun forever (CWE-835, 100% CPU).
 * The reader must instead reject the image with ARCHIVE_FATAL while
 * enumerating headers.
 */
DEFINE_TEST(test_read_format_iso_rockridge_cl_re_cycle)
{
	const char *refname = "test_read_format_iso_rockridge_cl_re_cycle.iso.Z";
	struct archive *a;
	struct archive_entry *ae;
	int r;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/*
	 * Read headers until the cyclic CL/RE structure is reached.  If the
	 * guard is missing this loop never returns (the test times out); with
	 * the guard the reader stops with ARCHIVE_FATAL.
	 */
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK)
		archive_read_data_skip(a);

	/* The cyclic CL/RE chain must be reported as a fatal error. */
	assertEqualIntA(a, ARCHIVE_FATAL, r);
	assert(archive_errno(a) != 0);
	assert(archive_error_string(a) != NULL);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

/*
 * Control: a well-formed Rock Ridge ISO that also uses the rr_moved / "RE"
 * relocation machinery but contains no cycle.  It must continue to parse
 * cleanly to EOF -- i.e. the cycle guard in rede_add_entry() must not
 * over-reject legitimate deep Rock Ridge relocation.
 */
DEFINE_TEST(test_read_format_iso_rockridge_cl_re_control)
{
	const char *refname =
	    "test_read_format_iso_rockridge_cl_re_control.iso.Z";
	struct archive *a;
	struct archive_entry *ae;
	int r, entries = 0;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK) {
		entries++;
		archive_read_data_skip(a);
	}

	/* Benign relocation image parses to completion, not to an error. */
	assertEqualIntA(a, ARCHIVE_EOF, r);
	assert(entries > 0);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
