/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2010 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_gid_gname)
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

	/* Again with both --gid and --gname */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive2 --gid=17 --gname=foofoofoo --format=ustar file >stdout2.txt 2>stderr2.txt",
		testprog));
	assertEmptyFile("stdout2.txt");
	assertEmptyFile("stderr2.txt");
	data = slurpfile(&s, "archive2");
	/* Should force gid and gname fields in ustar header. */
	assertEqualMem(data + 116, "000021 \0", 8);
	assertEqualMem(data + 297, "foofoofoo\0", 10);
	free(data);

	/* Again with just --gname */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive4 --gname=foofoofoo --format=ustar file >stdout4.txt 2>stderr4.txt",
		testprog));
	assertEmptyFile("stdout4.txt");
	assertEmptyFile("stderr4.txt");
	data = slurpfile(&s, "archive4");
	/* Gid should be unchanged from original reference. */
	assertEqualMem(data + 116, reference + 116, 8);
	assertEqualMem(data + 297, "foofoofoo\0", 10);
	free(data);
	free(reference);

	/* Again with --gid  and force gname to empty. */
	failure("Error invoking %s c", testprog);
	assertEqualInt(0,
	    systemf("%s cf archive3 --gid=17 --gname= --format=ustar file >stdout3.txt 2>stderr3.txt",
		testprog));
	assertEmptyFile("stdout3.txt");
	assertEmptyFile("stderr3.txt");
	data = slurpfile(&s, "archive3");
	assertEqualMem(data + 116, "000021 \0", 8);
	/* Gname field in ustar header should be empty. */
	assertEqualMem(data + 297, "\0", 1);
	free(data);

	/* TODO: It would be nice to verify that --gid= by itself
	 * will look up the associated gname and use that, but
	 * that requires some system-specific code. */

	/* TODO: It would be nice to verify that --gid= will
	 * leave the gname field blank if the specified gid
	 * isn't used on the local system. */
}
