/*-
 * Copyright (c) 2018,2019 Joseph Koshy
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

#ifndef	_LIBTEST_DRIVER_H_
#define	_LIBTEST_DRIVER_H_

#include <sys/queue.h>

#include <limits.h>
#include <stdbool.h>

#include "_elftc.h"

#include "test.h"

#define	TEST_SEARCH_PATH_ENV_VAR	"TEST_PATH"
#define	TEST_TMPDIR_ENV_VAR		"TEST_TMPDIR"

/*
 * Run time data strucrures.
 */

/* The completion status for a test run */
enum test_run_status {
	/*
	 * All test cases were successfully invoked, and all their contained
	 * test purposes passed.
	 */
	TR_STATUS_PASS = 0,

	/*
	 * All test cases were successfully invoked but at least one test
	 * function reported a failure.
	 */
	TR_STATUS_FAIL = 1,

	/*
	 * At least one test case reported an error during its set up or tear
	 * down phase.
	 */
	TR_STATUS_ERROR = 2
};

/*
 * The 'style' of the run determines the manner in which the test
 * executable reports test status.
 */
enum test_run_style {
	/* Libtest semantics. */
	TR_STYLE_LIBTEST,

	/*
	 * Be compatible with the Test Anything Protocol
	 * (http://testanything.org/).
	 */
	TR_STYLE_TAP,

	/* Be compatible with NetBSD ATF(9). */
	TR_STYLE_ATF
};

/*
 * Structures used for selecting tests.
 */
struct test_function_selector {
	const struct test_function_descriptor *tfs_descriptor;

	STAILQ_ENTRY(test_function_selector) tfs_next;
	int	tfs_is_selected;
};

STAILQ_HEAD(test_function_selector_list, test_function_selector);

struct test_case_selector {
	const struct test_case_descriptor	*tcs_descriptor;
	STAILQ_ENTRY(test_case_selector)	tcs_next;
	struct test_function_selector_list	tcs_functions;
	int					tcs_selected_count;
};

/*
 * The action being requested of the test driver.
 */
enum test_run_action {
	TEST_RUN_EXECUTE,	/* Execute the selected tests. */
	TEST_RUN_LIST,		/* Only list tests. */
};

STAILQ_HEAD(test_case_selector_list, test_case_selector);

/*
 * Runtime directories to look up data files.
 */
struct test_search_path_entry {
	char *tsp_directory;
	STAILQ_ENTRY(test_search_path_entry)	tsp_next;
};

STAILQ_HEAD(test_search_path_list, test_search_path_entry);

/*
 * Used to track flags that were explicity set on the command line.
 */
enum test_run_flags {
	TRF_BASE_DIRECTORY = 1U << 0,
	TRF_EXECUTION_TIME =  1U << 1,
	TRF_ARTEFACT_ARCHIVE = 1U << 2,
	TRF_NAME = 1U << 3,
	TRF_SEARCH_PATH = 1U << 4,
	TRF_EXECUTION_STYLE = 1U << 5,
};

/*
 * Parameters for the run.
 */
struct test_run {
	/*
	 * Flags tracking the options which were explicitly set.
	 *
	 * This field is a bitmask formed of 'enum test_run_flags' values.
	 */
	unsigned int		tr_commandline_flags;

	/* What the test run should do. */
	enum test_run_action	tr_action;

	/* The desired behavior of the test harness. */
	enum test_run_style	tr_style;

	/* The desired verbosity level. */
	int			tr_verbosity;

	/* An optional name assigned by the user for this test run. */
	char			*tr_name;

	/*
	 * The absolute path to the directory under which the test is
	 * to be run.
	 *
	 * Each test case will be invoked in some subdirectory of this
	 * directory.
	 */
	char			*tr_runtime_base_directory;

	/*
	 * The test timeout in seconds.
	 *
	 * A value of zero indicates that the test driver should wait
	 * indefinitely for tests.
	 */
	long			tr_max_seconds_per_test;

	/*
	 * If not NULL, An absolute pathname to an archive that will hold
	 * the artefacts created by a test run.
	 */
	char			*tr_artefact_archive;

	/*
	 * Directories to use when resolving non-absolute data file
	 * names.
	 */
	struct test_search_path_list tr_search_path;

	/* All tests selected for this run. */
	struct	test_case_selector_list	tr_test_cases;
};

#ifdef	__cplusplus
extern "C" {
#endif
struct test_run	*test_driver_allocate_run(void);
bool		test_driver_add_search_path(struct test_run *,
    const char *search_path);
void		test_driver_free_run(struct test_run *);
bool		test_driver_is_directory(const char *);
bool		test_driver_finish_run_initialization(struct test_run *,
    const char *argv0);
#ifdef	__cplusplus
}
#endif

#endif	/* _LIBTEST_DRIVER_H_ */
