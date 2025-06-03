/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2010 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_uid_uname)
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

	/* Again with both --uid and --uname */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive2 --uid=65123 --uname=foofoofoo --format=ustar file >stdout2.txt 2>stderr2.txt",
		testprog));
	assertEmptyFile("stdout2.txt");
	assertEmptyFile("stderr2.txt");
	data = slurpfile(&s, "archive2");
	/* Should force uid and uname fields in ustar header. */
	assertEqualMem(data + 108, "177143 \0", 8);
	assertEqualMem(data + 265, "foofoofoo\0", 10);
	free(data);

	/* Again with just --uid */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive3 --uid=65123 --format=ustar file >stdout3.txt 2>stderr3.txt",
		testprog));
	assertEmptyFile("stdout3.txt");
	assertEmptyFile("stderr3.txt");
	data = slurpfile(&s, "archive3");
	assertEqualMem(data + 108, "177143 \0", 8);
	/* Uname field in ustar header should be empty. */
	assertEqualMem(data + 265, "\0", 1);
	free(data);

	/* Again with just --uname */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive4 --uname=foofoofoo --format=ustar file >stdout4.txt 2>stderr4.txt",
		testprog));
	assertEmptyFile("stdout4.txt");
	assertEmptyFile("stderr4.txt");
	data = slurpfile(&s, "archive4");
	/* Uid should be unchanged from original reference. */
	assertEqualMem(data + 108, reference + 108, 8);
	assertEqualMem(data + 265, "foofoofoo\0", 10);
	free(data);

	free(reference);
}
