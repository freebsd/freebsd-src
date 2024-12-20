/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2009 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

/*
 * Regression test:  We used to get a bogus error message when we
 * asked tar to copy entries out of an empty archive.  See
 * Issue 51 on libarchive.googlecode.com for details.
 */
DEFINE_TEST(test_empty_mtree)
{
	int r;

	assertMakeFile("test1.mtree", 0777, "#mtree\n");

	r = systemf("%s cf test1.tar @test1.mtree >test1.out 2>test1.err",
	    testprog);
	failure("Error invoking %s cf", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("test1.out");
	assertEmptyFile("test1.err");
}
