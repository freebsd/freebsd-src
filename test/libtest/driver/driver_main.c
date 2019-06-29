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

/*
 * This file defines a "main()" that invokes (or lists) the tests that were
 * linked into the current executable.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include "_elftc.h"

#include "test.h"
#include "test_case.h"

#include "driver.h"

#if defined(ELFTC_VCSID)
ELFTC_VCSID("$Id$");
#endif

enum selection_scope {
	SCOPE_TEST_CASE = 0,	/* c:STRING */
	SCOPE_TEST_FUNCTION,	/* f:STRING */
	SCOPE_TAG,		/* t:STRING */
};

/* Selection list entry. */
struct selection_option {
	STAILQ_ENTRY(selection_option)	so_next;

	/* The text to use for matching. */
	const char	*so_pattern;

	/*
	 * Whether matched test and test cases should be selected
	 * (if false) or deselected (if true).
	 */
	bool		so_select_tests;

	/* The kind of information to match. */
	enum selection_scope	so_selection_scope;
};

/* All selection options specified. */
STAILQ_HEAD(selection_option_list, selection_option);

static struct selection_option *
parse_selection_option(const char *option)
{
	int scope_char;
	bool select_tests;
	enum selection_scope scope;
	struct selection_option *so;

	scope_char = '\0';
	select_tests = true;
	scope = SCOPE_TEST_CASE;

	/* Deselection patterns start with a '-'. */
	if (*option == '-') {
		select_tests = false;
		option++;
	}

	/*
	 * If a scope was not specified, the selection scope defaults
	 * to SCOPE_TEST_CASE.
	 */
	if (strchr(option, ':') == NULL)
		scope_char = 'c';
	else {
		scope_char = *option++;
		if (*option != ':')
			return (NULL);
		option++;	/* Skip over the ':'. */
	}

	if (*option == '\0')
		return (NULL);

	switch (scope_char) {
	case 'c':
		scope = SCOPE_TEST_CASE;
		break;
	case 'f':
		scope = SCOPE_TEST_FUNCTION;
		break;
	case 't':
		scope = SCOPE_TAG;
		break;
	default:
		return (NULL);
	}

	so = calloc(1, sizeof(*so));
	so->so_pattern = option;
	so->so_selection_scope = scope;
	so->so_select_tests = select_tests;

	return (so);
}

/* Test execution styles. */
struct style_entry {
	enum test_run_style	se_style;
	const char		*se_name;
};

static const struct style_entry known_styles[] = {
	{ TR_STYLE_LIBTEST, "libtest" },
	{ TR_STYLE_TAP, "tap" },
	{ TR_STYLE_ATF, "atf" }
};

/*
 * Parse a test run style.
 *
 * This function returns true if the run style was recognized, or
 * false otherwise.
 */
static bool
parse_run_style(const char *option, enum test_run_style *run_style)
{
	size_t n;

	for (n = 0; n < sizeof(known_styles) / sizeof(known_styles[0]); n++) {
		if (strcasecmp(option, known_styles[n].se_name) == 0) {
			*run_style = known_styles[n].se_style;
			return (true);
		}
	}

	return (false);
}

/*
 * Return the canonical spelling of a test execution style.
 */
static const char *
to_execution_style_name(enum test_run_style run_style)
{
	size_t n;

	for (n = 0; n < sizeof(known_styles) / sizeof(known_styles[0]); n++) {
		if (known_styles[n].se_style == run_style)
			return (known_styles[n].se_name);
	}

	return (NULL);
}

/*
 * Parse a string value containing a positive integral number.
 */
static bool
parse_execution_time(const char *option, long *execution_time) {
	char *end;
	long value;

	if (option == NULL || *option == '\0')
		return (false);

	value = strtol(option, &end, 10);

	/* Check for parse errors. */
	if (*end != '\0')
		return (false);

	/* Reject negative numbers. */
	if (value < 0)
		return (false);

	/* Check for overflows during parsing. */
	if (value == LONG_MAX && errno == ERANGE)
		return (false);

	*execution_time = value;

	return (true);
}

/*
 * Match the names of test cases.
 *
 * In the event of a match, then the selection state specifed in
 * 'option' is applied to all the test functions in the test case.
 */
static void
match_test_cases(struct selection_option *option,
    struct test_case_selector *tcs)
{
	const struct test_case_descriptor *tcd;
	struct test_function_selector *tfs;

	tcd = tcs->tcs_descriptor;

	if (fnmatch(option->so_pattern, tcd->tc_name, 0))
		return;

	STAILQ_FOREACH(tfs, &tcs->tcs_functions, tfs_next)
		tfs->tfs_is_selected = option->so_select_tests;
}

/*
 * Match the names of test functions.
 */
static void
match_test_functions(struct selection_option *option,
    struct test_case_selector *tcs)
{
	struct test_function_selector *tfs;
	const struct test_function_descriptor *tfd;

	STAILQ_FOREACH(tfs, &tcs->tcs_functions, tfs_next) {
		tfd = tfs->tfs_descriptor;

		if (fnmatch(option->so_pattern, tfd->tf_name, 0))
			continue;

		tfs->tfs_is_selected = option->so_select_tests;
	}
}

/*
 * Helper: returns true if the specified text matches any of the
 * entries in the array 'tags'.
 */
static bool
match_tags_helper(const char *pattern, const char *tags[])
{
	const char **tag;

	if (!tags)
		return (false);

	for (tag = tags; *tag && **tag != '\0'; tag++) {
		if (!fnmatch(pattern, *tag, 0))
			return (true);
	}

	return (false);
}

/*
 * Match tags.
 *
 * Matches against test case tags apply to all the test
 * functions in the test case.
 *
 * Matches against test function tags apply to the matched
 * test function only.
 */
static void
match_tags(struct selection_option *option,
    struct test_case_selector *tcs)
{
	const struct test_case_descriptor *tcd;
	const struct test_function_descriptor *tfd;
	struct test_function_selector *tfs;

	tcd = tcs->tcs_descriptor;

	/*
	 * If the tag in the option matches a tag associated with
	 * a test case, then we set all of the test case's functions
	 * to the specified selection state.
	 */
	if (match_tags_helper(option->so_pattern, tcd->tc_tags)) {
		STAILQ_FOREACH(tfs, &tcs->tcs_functions, tfs_next)
			tfs->tfs_is_selected = option->so_select_tests;
		return;
	}

	/*
	 * Otherwise, check the tag against the tags for each function
	 * in the test case and set the selection state of each matched
	 * function.
	 */
	STAILQ_FOREACH(tfs, &tcs->tcs_functions, tfs_next) {
		tfd = tfs->tfs_descriptor;
		if (match_tags_helper(option->so_pattern, tfd->tf_tags))
			tfs->tfs_is_selected = option->so_select_tests;
	}
}

/*
 * Add the selected tests to the test run.
 *
 * The memory used by the options list is returned to the system when this
 * function completes.
 */
static void
select_tests(struct test_run *tr,
    struct selection_option_list *selections)
{
	int i, j;
	struct selection_option *selection;
	const struct test_case_descriptor *tcd;
	struct test_case_selector *tcs;
	struct test_function_selector *tfs;
	bool default_selection_state;
	int selected_count;

	default_selection_state = STAILQ_EMPTY(selections);

	/*
	 * Set up runtime descriptors.
	 */
	for (i = 0; i < test_case_count; i++) {
		if ((tcs = calloc(1, sizeof(*tcs))) == NULL)
			err(EX_OSERR, "cannot allocate a test-case selector");
		STAILQ_INSERT_TAIL(&tr->tr_test_cases, tcs, tcs_next);
		STAILQ_INIT(&tcs->tcs_functions);

		tcd = &test_cases[i];

		tcs->tcs_descriptor = tcd;

		for (j = 0; j < tcd->tc_count; j++) {
			if ((tfs = calloc(1, sizeof(*tfs))) == NULL)
				err(EX_OSERR, "cannot allocate a test "
				    "function selector");
			STAILQ_INSERT_TAIL(&tcs->tcs_functions, tfs, tfs_next);

			tfs->tfs_descriptor = tcd->tc_tests + j;
			tfs->tfs_is_selected = default_selection_state;
		}
	}

	/*
	 * Set or reset the selection state based on the options.
	 */
	STAILQ_FOREACH(selection, selections, so_next) {
		STAILQ_FOREACH(tcs, &tr->tr_test_cases, tcs_next) {
			switch (selection->so_selection_scope) {
			case SCOPE_TEST_CASE:
				match_test_cases(selection, tcs);
				break;
			case SCOPE_TEST_FUNCTION:
				match_test_functions(selection, tcs);
				break;
			case SCOPE_TAG:
				match_tags(selection, tcs);
				break;
			}
		}
	}

	/*
	 * Determine the count of tests selected, for each test case.
	 */
	STAILQ_FOREACH(tcs, &tr->tr_test_cases, tcs_next) {
		selected_count = 0;
		STAILQ_FOREACH(tfs, &tcs->tcs_functions, tfs_next)
			selected_count += tfs->tfs_is_selected;
		tcs->tcs_selected_count = selected_count;
	}

	/* Free up the selection list. */
	while (!STAILQ_EMPTY(selections)) {
		selection = STAILQ_FIRST(selections);
		STAILQ_REMOVE_HEAD(selections, so_next);
		free(selection);
	}
}

/*
 * Translate a file name to absolute form.
 *
 * The caller needs to free the returned pointer.
 */
static char *
to_absolute_path(const char *filename)
{
	size_t space_needed;
	char *absolute_path;
	char current_directory[PATH_MAX];

	if (filename == NULL || *filename == '\0')
		return (NULL);
	if (*filename == '/')
		return strdup(filename);

	if (getcwd(current_directory, sizeof(current_directory)) == NULL)
		err(1, "getcwd failed");

	/* Reserve space for the slash separator and the trailing NUL. */
	space_needed = strlen(current_directory) + strlen(filename) + 2;
	if ((absolute_path = malloc(space_needed)) == NULL)
		err(1, "malloc failed");
	if (snprintf(absolute_path, space_needed, "%s/%s", current_directory,
	    filename) != (int) (space_needed - 1))
		err(1, "snprintf failed");
	return (absolute_path);
}


/*
 * Display run parameters.
 */

#define	FIELD_NAME_WIDTH	24
#define	INFOLINE(NAME, FLAG, FORMAT, ...)	do {			\
		printf("I %c %-*s " FORMAT,				\
		    (FLAG) ? '!' : '.',					\
		    FIELD_NAME_WIDTH, NAME, __VA_ARGS__);		\
	} while (0)

static void
show_run_header(const struct test_run *tr)
{
	time_t start_time;
	struct test_search_path_entry *path_entry;

	if (tr->tr_verbosity == 0)
		return;

	INFOLINE("test-run-name", tr->tr_commandline_flags & TRF_NAME,
	    "%s\n", tr->tr_name);

	INFOLINE("test-execution-style",
	    tr->tr_commandline_flags & TRF_EXECUTION_STYLE,
	    "%s\n", to_execution_style_name(tr->tr_style));

	if (!STAILQ_EMPTY(&tr->tr_search_path)) {
		INFOLINE("test-search-path",
		    tr->tr_commandline_flags & TRF_SEARCH_PATH,
		    "%c", '[');
		STAILQ_FOREACH(path_entry, &tr->tr_search_path, tsp_next) {
			printf(" %s", path_entry->tsp_directory);
		}
		printf(" ]\n");
	}

	INFOLINE("test-run-base-directory",
	    tr->tr_commandline_flags & TRF_BASE_DIRECTORY,
	    "%s\n", tr->tr_runtime_base_directory);

	if (tr->tr_artefact_archive) {
		INFOLINE("test-artefact-archive",
		    tr->tr_commandline_flags & TRF_ARTEFACT_ARCHIVE,
		    "%s\n", tr->tr_artefact_archive);
	}

	printf("I %c %-*s ",
	    tr->tr_commandline_flags & TRF_EXECUTION_TIME ? '=' : '.',
	    FIELD_NAME_WIDTH, "test-execution-time");
	if (tr->tr_max_seconds_per_test == 0)
		printf("unlimited\n");
	else
		printf("%lu\n", tr->tr_max_seconds_per_test);

	printf("I %% %-*s %d\n", FIELD_NAME_WIDTH, "test-case-count",
	    test_case_count);

	if (tr->tr_action == TEST_RUN_EXECUTE) {
		start_time = time(NULL);
		printf("I %% %-*s %s", FIELD_NAME_WIDTH,
		    "test-run-start-time", ctime(&start_time));
	}
}

static void
show_run_trailer(const struct test_run *tr)
{
	time_t end_time;

	if (tr->tr_verbosity == 0)
		return;

	if (tr->tr_action == TEST_RUN_EXECUTE) {
		end_time = time(NULL);
		printf("I %% %-*s %s", FIELD_NAME_WIDTH, "test-run-end-time",
		    asctime(localtime(&end_time)));
	}
}

#undef	INFOLINE
#undef	FIELD_HEADER_WIDTH

/*
 * Helper: returns a character indicating the selection status for
 * a test case.  This character is as follows:
 *
 * - "*" all test functions in the test case were selected.
 * - "+" some test functions in the test case were selected.
 * - "-" no test functions from the test case were selected.
 */
static int
get_test_case_status(const struct test_case_selector *tcs)
{
	if (tcs->tcs_selected_count == 0)
		return '-';
	if (tcs->tcs_selected_count == tcs->tcs_descriptor->tc_count)
		return '*';
	return '?';
}

/*
 * Helper: print out a comma-separated list of tags.
 */
static void
show_tags(int indent, const char *tags[])
{
	const char **tag;

	printf("%*c: ", indent, ' ');
	for (tag = tags; *tag && **tag != '\0';) {
		printf("%s", *tag++);
		if (*tag && **tag != '\0')
			printf(",");
	}
	printf("\n");
}

/*
 * Display a test case descriptor.
 */
static void
show_test_case(struct test_run *tr, const struct test_case_selector *tcs)
{
	const struct test_case_descriptor *tcd;
	int prefix_char;

	prefix_char = get_test_case_status(tcs);
	tcd = tcs->tcs_descriptor;

	printf("C %c %s\n", prefix_char, tcd->tc_name);

	if (tr->tr_verbosity > 0 && tcd->tc_tags != NULL)
		show_tags(2, tcd->tc_tags);

	if (tr->tr_verbosity > 1 && tcd->tc_description)
		printf("  & %s\n", tcd->tc_description);
}

static void
show_test_function(struct test_run *tr,
    const struct test_function_selector *tfs)
{
	const struct test_function_descriptor *tfd;
	int selection_char;

	selection_char = tfs->tfs_is_selected ? '*' : '-';
	tfd = tfs->tfs_descriptor;

	printf("  F %c %s\n", selection_char, tfd->tf_name);

	if (tr->tr_verbosity > 0 && tfd->tf_tags != NULL)
		show_tags(4, tfd->tf_tags);

	if (tr->tr_verbosity > 1 && tfd->tf_description)
		printf("    & %s\n", tfd->tf_description);
}

static int
show_listing(struct test_run *tr)
{
	const struct test_case_selector *tcs;
	const struct test_function_selector *tfs;

	STAILQ_FOREACH(tcs, &tr->tr_test_cases, tcs_next) {
		show_test_case(tr, tcs);
		STAILQ_FOREACH(tfs, &tcs->tcs_functions, tfs_next)
			show_test_function(tr, tfs);
	}

	return (EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
	struct test_run *tr;
	int exit_code, option;
	enum test_run_style run_style;
	struct selection_option *selector;
	struct selection_option_list selections =
	    STAILQ_HEAD_INITIALIZER(selections);

	tr = test_driver_allocate_run();

	/* Parse arguments. */
	while ((option = getopt(argc, argv, ":R:T:c:ln:p:s:t:v")) != -1) {
		switch (option) {
		case 'R':	/* Test runtime directory. */
			if (!test_driver_is_directory(optarg))
				errx(EX_USAGE, "option -%c: argument \"%s\" "
				    "does not name a directory.", option,
				    optarg);
			tr->tr_runtime_base_directory = realpath(optarg, NULL);
			if (tr->tr_runtime_base_directory == NULL)
				err(1, "realpath failed for \"%s\"", optarg);
			tr->tr_commandline_flags |= TRF_BASE_DIRECTORY;
			break;
		case 'T':	/* Max execution time for a test function. */
			if (!parse_execution_time(
			    optarg, &tr->tr_max_seconds_per_test))
				errx(EX_USAGE, "option -%c: argument \"%s\" "
				    "is not a valid execution time value.",
				    option, optarg);
			tr->tr_commandline_flags |= TRF_EXECUTION_TIME;
			break;
		case 'c':	/* The archive holding artefacts. */
			tr->tr_artefact_archive = to_absolute_path(optarg);
			tr->tr_commandline_flags |= TRF_ARTEFACT_ARCHIVE;
			break;
		case 'l':	/* List matching tests. */
			tr->tr_action = TEST_RUN_LIST;
			break;
		case 'n':	/* Test run name. */
			if (tr->tr_name)
				free(tr->tr_name);
			tr->tr_name = strdup(optarg);
			tr->tr_commandline_flags |= TRF_NAME;
			break;
		case 'p':	/* Add a search path entry. */
			if (!test_driver_add_search_path(tr, optarg))
				errx(EX_USAGE, "option -%c: argument \"%s\" "
				    "does not name a directory.", option,
				    optarg);
			tr->tr_commandline_flags |= TRF_SEARCH_PATH;
			break;
		case 's':	/* Test execution style. */
			if (!parse_run_style(optarg, &run_style))
				errx(EX_USAGE, "option -%c: argument \"%s\" "
				    "is not a supported test execution style.",
				    option, optarg);
			tr->tr_style = run_style;
			tr->tr_commandline_flags |= TRF_EXECUTION_STYLE;
			break;
		case 't':	/* Test selection option. */
			if ((selector = parse_selection_option(optarg)) == NULL)
				errx(EX_USAGE, "option -%c: argument \"%s\" "
				    "is not a valid selection pattern.",
				    option, optarg);
			STAILQ_INSERT_TAIL(&selections, selector, so_next);
			break;
		case 'v':
			tr->tr_verbosity++;
			break;
		case ':':
			errx(EX_USAGE,
			    "ERROR: option -%c requires an argument.", optopt);
			break;
		case '?':
			errx(EX_USAGE,
			    "ERROR: unrecognized option -%c", optopt);
			break;
		default:
			errx(EX_USAGE, "ERROR: unspecified error.");
			break;
		}
	}

	/*
	 * Set unset fields of the test run descriptor to their
	 * defaults.
	 */
	if (!test_driver_finish_run_initialization(tr, argv[0]))
		err(EX_OSERR, "cannot initialize test driver");

	/* Choose tests and test cases to act upon. */
	select_tests(tr, &selections);

	assert(STAILQ_EMPTY(&selections));

	show_run_header(tr);

	/* Perform the requested action. */
	switch (tr->tr_action) {
	case TEST_RUN_LIST:
		exit_code = show_listing(tr);
		break;

	case TEST_RUN_EXECUTE:
	default:
		/* Not yet implemented. */
		exit_code = EX_UNAVAILABLE;
	}

	show_run_trailer(tr);

	test_driver_free_run(tr);

	exit(exit_code);
}
