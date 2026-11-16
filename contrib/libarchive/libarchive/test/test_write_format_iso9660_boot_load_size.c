/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Martin Mirchev
 * All rights reserved.
 */
#include "test.h"

/*
 * Check the range of the boot-load-size option, which must be 1..65535.
 */

static int
set_boot_load_size(const char *value)
{
	struct archive *a;
	int r;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_iso9660(a));
	r = archive_write_set_option(a, "iso9660", "boot-load-size", value);
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	return (r);
}

DEFINE_TEST(test_write_format_iso9660_boot_load_size)
{
	/* Valid values. */
	assertEqualInt(ARCHIVE_OK, set_boot_load_size("1"));
	assertEqualInt(ARCHIVE_OK, set_boot_load_size("4"));
	assertEqualInt(ARCHIVE_OK, set_boot_load_size("05"));
	assertEqualInt(ARCHIVE_OK, set_boot_load_size("65535"));

	/* Out of range or malformed values. */
	assert(set_boot_load_size("0") != ARCHIVE_OK);
	assert(set_boot_load_size("65536") != ARCHIVE_OK);
	assert(set_boot_load_size("-5") != ARCHIVE_OK);
	assert(set_boot_load_size("-") != ARCHIVE_OK);
	assert(set_boot_load_size("") != ARCHIVE_OK);
	assert(set_boot_load_size("5x") != ARCHIVE_OK);
}
