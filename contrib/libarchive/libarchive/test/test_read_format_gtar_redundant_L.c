/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

#include <string.h>

DEFINE_TEST(test_read_format_gtar_redundant_L)
{
	const char *refname = "test_read_format_gtar_redundant_L.tar.Z";
	struct archive *a;
	struct archive_entry *ae;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));

	/* First file has redundant 'L' headers; this should prompt
	 * a suitable ARCHIVE_WARN message */
	assertEqualIntA(a, ARCHIVE_WARN, archive_read_next_header(a, &ae));
	assertEqualInt(archive_errno(a), -1);
	assert(strstr(archive_error_string(a), "Redundant 'L'") != NULL);

	/* End of archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify archive format. */
	assertEqualIntA(a, ARCHIVE_FILTER_COMPRESS, archive_filter_code(a, 0));
	assertEqualIntA(a, ARCHIVE_FORMAT_TAR_GNUTAR, archive_format(a));

	/* Close the archive. */
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
