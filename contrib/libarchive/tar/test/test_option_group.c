/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2010 Tim Kientzle
 * Copyright (c) 2024 Haelwenn (lanodan) Monnier
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_group)
{
	char *reference, *data;
	size_t s;

	assertUmask(0);
	assertMakeFile("file", 0644, "1234567890");

	/* Create archive with no special options. */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive1 --format=ustar file >stdout1.txt 2>stderr1.txt",
		testprog));
	assertEmptyFile("stdout1.txt");
	assertEmptyFile("stderr1.txt");
	reference = slurpfile(&s, "archive1");

	/* Create archive with --group (numeric) */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive2 --group=17 --format=ustar file >stdout2.txt 2>stderr2.txt",
		testprog));
	assertEmptyFile("stdout2.txt");
	assertEmptyFile("stderr2.txt");
	data = slurpfile(&s, "archive2");
	assertEqualMem(data + 116, "000021 \0", 8);
	/* Gname field in ustar header should be empty. */
	assertEqualMem(data + 297, "\0", 1);
	free(data);

	/* Again with --group (name) */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive3 --group=foofoofoo --format=ustar file >stdout3.txt 2>stderr3.txt",
		testprog));
	assertEmptyFile("stdout3.txt");
	assertEmptyFile("stderr3.txt");
	data = slurpfile(&s, "archive3");
	/* Gid should be unchanged from original reference. */
	assertEqualMem(data + 116, reference + 116, 8);
	assertEqualMem(data + 297, "foofoofoo\0", 10);
	free(data);

	/* Again with --group (name:id) */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive4 --group=foofoofoo:17 --format=ustar file >stdout4.txt 2>stderr4.txt",
		testprog));
	assertEmptyFile("stdout4.txt");
	assertEmptyFile("stderr4.txt");
	data = slurpfile(&s, "archive4");
	assertEqualMem(data + 116, "000021 \0", 8);
	assertEqualMem(data + 297, "foofoofoo\0", 10);
	free(data);

	free(reference);
}
