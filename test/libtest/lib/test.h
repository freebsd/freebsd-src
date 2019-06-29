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

#ifndef	_LIBTEST_TEST_H_
#define	_LIBTEST_TEST_H_

/*
 * The return values from test functions.
 *
 * - TEST_PASS : The assertion(s) in the test function passed.
 * - TEST_FAIL : At least one assertion in the test function failed.
 * - TEST_UNRESOLVED : The assertions in the test function could not be
 *                     checked for some reason.
 */
enum test_result {
	TEST_PASS = 0,
	TEST_FAIL = 1,
	TEST_UNRESOLVED = 2
};

/*
 * The return values from test case set up and tear down functions.
 *
 * - TEST_CASE_OK : The set up or tear down function was successful.
 * - TEST_CASE_ERROR : Set up or tear down actions could not be completed.
 *
 * If a test case set up function returns TEST_CASE_ERROR then:
 * - The test functions in the test case will not be run.
 * - The test case's tear down function will not be invoked.
 * - The test run as a whole will be treated as being in error.
 *
 * If a test case tear down function returns a TEST_CASE_ERROR, then
 * the test run as a whole be treated as being in error.
 */
enum test_case_status {
	TEST_CASE_OK = 0,
	TEST_CASE_ERROR = 1
};

/*
 * A 'test_case_state' is a handle to resources shared by the test functions
 * that make up a test case. A test_case_state is allocated by the test case
 * set up function and is deallocated by the test case tear down function.
 *
 * The test(3) framework treats a 'test_case_state' as an opaque value.
 */
typedef	void *test_case_state;

/*
 * Test case and test function descriptions, and convenience macros
 * to define these.
 */
typedef const char test_case_description[];

#if	!defined(TEST_CASE_DESCRIPTION)
#define	TEST_CASE_DESCRIPTION(NAME) test_case_description tc_description_##NAME
#endif

typedef const char test_description[];

#if	!defined(TEST_DESCRIPTION)
#define	TEST_DESCRIPTION(NAME) test_description tf_description_##NAME
#endif

/*
 * Test case and test function tags, and convenience macros to define
 * these.
 */
typedef const char *test_case_tags[];

#if	!defined(TEST_CASE_TAGS)
#define	TEST_CASE_TAGS(NAME)	test_case_tags tc_tags_##NAME
#endif

typedef const char *test_tags[];

#if	!defined(TEST_TAGS)
#define	TEST_TAGS(NAME)		test_tags tf_tags_##NAME
#endif

/*
 * A test case set up function.
 *
 * If defined for a test case, this function will be called prior to
 * the execution of an of the test functions within the test cae. Test
 * case execution will be aborted if the function returns any value other
 * than TEST_CASE_OK.
 *
 * The function can set '*state' to a memory area holding test state to be
 * passed to test functions.
 *
 * If the test case does not define a set up function, then a default
 * no-op set up function will be used.
 */
typedef	enum test_case_status	test_case_setup_function(
    test_case_state *state);

/*
 * A test function.
 *
 * This function will be invoked with the state that had been set by the
 * test case set up function. The function returns TEST_PASS to report that
 * its test succeeded or TEST_FAIL otherwise. In the event the test could
 * not be executed, it can return TEST_UNRESOLVED.
 */
typedef	enum test_result	test_function(test_case_state state);

/*
 * A test case tear down function.
 *
 * If defined for a test case, this function will be called after the
 * execution of the test functions in the test case.  It is passed the
 * state that had been allocated by the test case set up function, and is
 * responsible for deallocating the resources that the set up function
 * had allocated.
 */
typedef enum test_case_status	test_case_teardown_function(
    test_case_state state);

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Write a progress report to the test log.
 *
 * This function takes a printf(3)-like format string and associated
 * arguments.
 */
int	test_report_progress(const char *format, ...);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBTEST_TEST_H_ */
