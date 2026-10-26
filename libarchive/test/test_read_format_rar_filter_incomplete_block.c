/*-
 * Copyright (c) 2026 Alex J.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "test.h"

/*
 * A RAR (v4) archive that declares a VM filter and then begins a PPMd block
 * before the filtered region has been fully produced.  libarchive does not
 * support a filter spanning into a PPMd block, so run_filters() refuses to
 * run it and the read fails.
 *
 * The failure itself is expected.  What this test pins down is that the
 * reader reports it: archive_read_data() must return a failure AND leave an
 * error message behind, the way every other failure path in the reader does.
 * It used to return failure with archive_error_string() still NULL.
 *
 * The archive is ordinary output of the RAR 6.24 encoder:
 *     rar a -ma4 -m5 -mct+ archive.rar input.bin
 */
DEFINE_TEST(test_read_format_rar_filter_incomplete_block)
{
	const char *refname = "test_read_format_rar_filter_incomplete_block.rar";
	struct archive *a;
	struct archive_entry *ae;
	char buff[4096];
	ssize_t r;

	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("f2500.bin", archive_entry_pathname(ae));

	/* The filter block is never fully decompressed, so the read fails. */
	r = archive_read_data(a, buff, sizeof(buff));
	assert(r < ARCHIVE_OK);

	/* A failed read must say why. */
	assert(archive_error_string(a) != NULL);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
