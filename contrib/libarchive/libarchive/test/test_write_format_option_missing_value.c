/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Martin Mirchev
 * All rights reserved.
 */
#include "test.h"

/*
 * Options that need a value must fail with a usable error message when
 * they are given without one (e.g. "zip:!threads" on the command line),
 * and must be accepted when they are given one.
 */
static void
check_missing_value(struct archive *a, const char *module, const char *key)
{
	const char *msg;

	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_format_option(a, module, key, NULL));
	msg = archive_error_string(a);
	failure("%s:%s without a value", module, key);
	assert(msg != NULL);
	if (msg != NULL) {
		failure("%s:%s message: %s", module, key, msg);
		assert(strstr(msg, "(null)") == NULL);
	}
}

DEFINE_TEST(test_write_format_option_missing_value)
{
	struct archive *a;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	check_missing_value(a, "zip", "threads");
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_format_option(a, "zip", "threads", "2"));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_7zip(a));
	check_missing_value(a, "7zip", "compression-level");
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_write_new()) != NULL);
	if (archive_write_set_format_xar(a) != ARCHIVE_OK) {
		skipping("xar writer is not supported on this platform");
	} else {
		check_missing_value(a, "xar", "threads");
		assertEqualIntA(a, ARCHIVE_OK,
		    archive_write_set_format_option(a, "xar", "threads", "4"));
	}
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}
