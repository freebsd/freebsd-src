/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Martin Mirchev
 * All rights reserved.
 */
#include "test.h"

/*
 * A warning while reading entry data from an @archive input (here an LHA
 * data CRC error) must be reported and reflected in the exit status.
 */
DEFINE_TEST(test_copy_data_warning)
{
	const char *refname = "test_copy_data_warning.lzh";
	int r;

	extract_reference_file(refname);

	r = systemf("%s -cf damaged.tar @%s >damaged.out 2>damaged.err",
	    testprog, refname);
	failure("The data CRC error must be reported in the exit status");
	assert(r != 0);
	assertNonEmptyFile("damaged.err");
}
