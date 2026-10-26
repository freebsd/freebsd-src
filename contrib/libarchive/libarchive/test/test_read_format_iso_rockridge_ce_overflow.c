/*-
 * Copyright (c) 2026 iGotYourBackMr
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
 * parse_rockridge() read the Rock Ridge "CE" location into an int32_t and
 * register_CE() turned it into a byte offset with
 *
 *	offset = ((uint64_t)location) * logical_block_size;
 *
 * Location 0xFFFFFFFF sign-extends to -1, so that product is
 * 2^64 - logical_block_size.  The bound test that followed,
 *
 *	offset + file->ce_offset + file->ce_size > iso9660->volume_size
 *
 * is wrapping uint64 arithmetic, so an entry choosing
 * ce_offset + ce_size == logical_block_size brought the sum to exactly 0 and
 * the test passed.  The entry was accepted and queued at an offset that never
 * matches current_position, so read_CE() silently never processed it.
 *
 * 0xFFFFFFFF is the only location for which this worked.  An earlier check
 * already rejects ce_offset + ce_size > logical_block_size, so for a location
 * that sign-extends to -k the wrapped sum is small only when k is 1 and
 * ce_offset + ce_size is exactly logical_block_size; every other location with
 * its top bit set left the sum near 2^64, which the bound test rejected.
 *
 * The reference image is a directory entry carrying exactly that "CE":
 * location 0xFFFFFFFF, ce_offset 0, ce_size 2048.
 *
 * Carrying the location as uint32_t is what fixes it: the offset becomes
 * 0x7FFFFFF800, nothing wraps, and the bound test rejects the entry the way it
 * was always meant to.  (The checked helpers register_CE() now uses make the
 * bound explicit, but their overflow branch cannot trigger for any value these
 * fields can hold: location is at most 2^32-1 and logical_block_size is read
 * with archive_le16dec, so the product stays below 2^48 and the sum below
 * 2^49.)
 */
DEFINE_TEST(test_read_format_iso_rockridge_ce_overflow)
{
	const char *refname = "test_read_format_iso_rockridge_ce_overflow.iso.Z";
	struct archive *a;
	struct archive_entry *ae;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* The malformed "CE" is parsed while reading the root directory, so
	 * the very first header read must fail.  Before the fix the entry was
	 * accepted and this returned ARCHIVE_OK. */
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));
	assertEqualString("Invalid parameter in SUSP \"CE\" extension",
	    archive_error_string(a));

	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
