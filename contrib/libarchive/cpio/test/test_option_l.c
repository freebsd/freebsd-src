/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_l)
{
	int r;

	/* Create a file. */
	assertMakeFile("f", 0644, "a");

	/* Copy the file to the "copy" dir. */
	r = systemf("echo f | %s -pd copy >copy.out 2>copy.err",
	    testprog);
	assertEqualInt(r, 0);

	/* Check that the copy is a true copy and not a link. */
	assertIsNotHardlink("f", "copy/f");

	/* Copy the file to the "link" dir with the -l option. */
	r = systemf("echo f | %s -pld link >link.out 2>link.err",
	    testprog);
	assertEqualInt(r, 0);

	/* Check that this is a link and not a copy. */
	assertIsHardlink("f", "link/f");
}
