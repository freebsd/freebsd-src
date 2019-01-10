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

#ifndef	_LIBTEST_TEST_RUNNER_H_
#define	_LIBTEST_TEST_RUNNER_H_

#include "test.h"

/*
 * These data structures and functions are used by test driver that
 * execute tests.
 */

/*
 * The completion status for a test run:
 *
 * - TESTRUN_PASS : All test cases were successfully invoked and all test
 *                  purposes in the test cases passed.
 * - TESTRUN_FAIL : All test cases were successfully invoked but at least
 *                  one test purpose reported a test failure.
 * - TESTRUN_ERROR : At least one test case reported an error during its
 *                   set up or tear down phase.
 */
enum testrun_status {
	TESTRUN_PASS = 0,
	TESTRUN_FAIL = 1,
	TESTRUN_ERROR = 2
};

/*
 * A single test function, with its associated tags and description.
 */
struct test_descriptor {
	const char	*t_name;	/* Test name. */
	const char	*t_description;	/* Test description. */
	const char	**t_tags;	/* Tags associated with the test. */
	test_function	*t_func;	/* The function to invoke. */
};

/*
 * A test case.
 */
struct test_case_descriptor {
	const char	*tc_name;	/* Test case name. */
	const char	*tc_description; /* Test case description. */
	const char	**tc_tags;	/* Any associated tags. */
	struct test_descriptor *tc_tests; /* The tests in this test case. */
};

/*
 * All test cases.
 */
extern struct test_case_descriptor test_cases[];

enum testrun_style {
	/* Libtest semantics. */
	TESTRUN_STYLE_LIBTEST,

	/*
	 * Be compatible with the Test Anything Protocol
	 * (http://testanything.org/).
	 */
	TESTRUN_STYLE_TAP,

	/* Be compatible with NetBSD ATF(9). */
	TESTRUN_STYLE_ATF
};

/*
 * Parameters for the run.
 */
struct test_run {
	/*
	 * An optional name assigned by the user for this test run.
	 *
	 * This name is reported in test logs and is not interpreted
	 * by the test harness.
	 */
	char *testrun_name;

	/* The source directory for the run. */
	char *testrun_source_directory;

	/* The directory in which the test is executing. */
	char *testrun_test_directory;
};

#ifdef	__cplusplus
extern "C" {
#endif
#ifdef	__cplusplus
}
#endif

#endif	/* _LIBTEST_TEST_RUNNER_H_ */
