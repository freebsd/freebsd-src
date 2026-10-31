/*-
 * Copyright (c) 2026 Anish Aggarwal
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

/*
 * A Joliet ISO whose Rock Ridge "CL"/"RE" relocation entries chain
 * two relocated directories onto each other: the RE directory "ydir"
 * is reparented under "xdir", and "xdir" is then reparented under
 * "ydir", so the parent pointers form a cycle (xdir -> ydir -> xdir).
 *
 * build_pathname_utf16be() walked the parent chain recursively with
 * no depth limit, so reading the header for "xdir" recursed forever
 * and exhausted the stack (SIGSEGV).  Its sibling build_pathname()
 * has limited recursion depth to 1000 since the fix for issue #522;
 * with the same limit in build_pathname_utf16be(),
 * archive_read_next_header() returns ARCHIVE_FATAL
 * ("Pathname is too long") instead of crashing.
 */
DEFINE_TEST(test_read_format_iso_joliet_utf16be_cycle)
{
	const char *refname = "test_read_format_iso_joliet_utf16be_cycle.iso.Z";
	struct archive *a;
	struct archive_entry *ae;
	int r;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* Read through entries until the cyclic entry is reached.
	 * With the fix this returns ARCHIVE_FATAL ("Pathname is too
	 * long") instead of recursing until the stack is exhausted. */
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK)
		archive_read_data_skip(a);
	assertEqualInt(ARCHIVE_FATAL, r);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
