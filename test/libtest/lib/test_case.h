/*-
 * Copyright (c) 2018, Joseph Koshy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_LIBTEST_TEST_CASE_H_
#define	_LIBTEST_TEST_CASE_H_

#include "test.h"

/*
 * These structures describe the test cases that are linked into a
 * test executable.
 */

/* A single test function, with its associated tags and description. */
struct test_function_descriptor {
	const char	*tf_name;	/* Test name. */
	const char	*tf_description; /* Test description. */
	const char	**tf_tags;	/* The tags for the test. */
	test_function	*tf_func;	/* The function to invoke. */
};

/* A test case, with its associated tests. */
struct test_case_descriptor {
	const char	*tc_name;	/* Test case name. */
	const char	*tc_description; /* Test case description. */
	const char	**tc_tags;	/* Any associated tags. */
	const struct test_function_descriptor *tc_tests; /* Contained tests. */
	const int	tc_count;	/* The number of tests. */
};

/* All test cases linked into the test binary. */
extern struct test_case_descriptor test_cases[];
extern const int test_case_count;

#endif	/* _LIBTEST_TEST_CASE_H_ */
