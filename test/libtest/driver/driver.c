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
 * The implementation of the test driver.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "driver.h"

#if defined(ELFTC_VCSID)
ELFTC_VCSID("$Id$");
#endif

#define	SYSTEM_TMPDIR_ENV_VAR		"TMPDIR"

bool
test_driver_add_search_path(struct test_run *tr, const char *directory_name)
{
	char *canonical_path;
	struct test_search_path_entry *entry;

	if (!test_driver_is_directory(directory_name))
		return (false);

	if ((canonical_path = realpath(directory_name, NULL)) == NULL)
		err(1, "Cannot determine the canonical path for \"%s\"",
		    directory_name);

	/* Look for, and ignore duplicates. */
	STAILQ_FOREACH(entry, &tr->tr_search_path, tsp_next) {
		if (strcmp(canonical_path, entry->tsp_directory) == 0)
			return (true);
	}

	entry = calloc(1, sizeof(*entry));
	entry->tsp_directory = canonical_path;

	STAILQ_INSERT_TAIL(&tr->tr_search_path, entry, tsp_next);

	return (true);
}

/*
 * Return an initialized test run descriptor.
 *
 * The caller should use test_driver_free_run() to release the returned
 * descriptor.
 */
struct test_run *
test_driver_allocate_run(void)
{
	struct test_run *tr;

	tr = calloc(sizeof(struct test_run), 1);
	tr->tr_action = TEST_RUN_EXECUTE;
	tr->tr_style = TR_STYLE_LIBTEST;
	STAILQ_INIT(&tr->tr_test_cases);
	STAILQ_INIT(&tr->tr_search_path);

	return (tr);
}

/*
 * Destroy an allocated test run descriptor.
 *
 * The passed in pointer should not be used after this function returns.
 */
void
test_driver_free_run(struct test_run *tr)
{
	struct test_search_path_entry *path_entry;
	struct test_case_selector *test_case_entry;
	struct test_function_selector *function_entry;

	free(tr->tr_runtime_base_directory);
	free(tr->tr_name);
	if (tr->tr_artefact_archive)
		free(tr->tr_artefact_archive);

	/* Free the search path list. */
	while (!STAILQ_EMPTY(&tr->tr_search_path)) {
		path_entry = STAILQ_FIRST(&tr->tr_search_path);
		STAILQ_REMOVE_HEAD(&tr->tr_search_path, tsp_next);
		free(path_entry);
	}

	/* Free the test selector list. */
	while (!STAILQ_EMPTY(&tr->tr_test_cases)) {
		test_case_entry = STAILQ_FIRST(&tr->tr_test_cases);
		STAILQ_REMOVE_HEAD(&tr->tr_test_cases, tcs_next);

		/* Free the linked test functions. */
		while (!STAILQ_EMPTY(&test_case_entry->tcs_functions)) {
			function_entry =
			    STAILQ_FIRST(&test_case_entry->tcs_functions);
			STAILQ_REMOVE_HEAD(&test_case_entry->tcs_functions,
			    tfs_next);

			free(function_entry);
		}

		free(test_case_entry);
	}

	free(tr);
}

/*
 * Populate unset fields of a struct test_run with defaults.
 */
bool
test_driver_finish_run_initialization(struct test_run *tr, const char *argv0)
{
	struct timeval tv;
	const char *basedir;
	const char *search_path;
	const char *last_component;
	char *argv0_copy, *path_copy, *path_element;
	char test_name[NAME_MAX];

	if (tr->tr_name == NULL) {
		/* Per POSIX, basename(3) can modify its argument. */
		argv0_copy = strdup(argv0);
		last_component = basename(argv0_copy);

		if (gettimeofday(&tv, NULL))
			return (false);

		(void) snprintf(test_name, sizeof(test_name), "%s+%ld%ld",
		    last_component, (long) tv.tv_sec, (long) tv.tv_usec);

		tr->tr_name = strdup(test_name);

		free(argv0_copy);
	}

	/*
	 * Select a base directory, if one was not specified.
	 */
	if (tr->tr_runtime_base_directory == NULL) {
		basedir = getenv(TEST_TMPDIR_ENV_VAR);
		if (basedir == NULL)
			basedir = getenv(SYSTEM_TMPDIR_ENV_VAR);
		if (basedir == NULL)
			basedir = "/tmp";
		tr->tr_runtime_base_directory = realpath(basedir, NULL);
		if (tr->tr_runtime_base_directory == NULL)
			err(1, "realpath(%s) failed", basedir);
	}

	/*
	 * Add the search paths specified by the environment variable
	 * 'TEST_PATH' to the end of the search list.
	 */
	if ((search_path = getenv(TEST_SEARCH_PATH_ENV_VAR)) != NULL &&
	    *search_path != '\0') {
		path_copy = strdup(search_path);
		path_element = strtok(path_copy, ":");
		do {
			if (!test_driver_add_search_path(tr, path_element))
				warnx("in environment variable \"%s\": path "
				    "\"%s\" does not name a directory.",
				    TEST_SEARCH_PATH_ENV_VAR, path_element);
		} while ((path_element = strtok(NULL, ":")) != NULL);
	}

	return (true);
}

/*
 * Helper: return true if the passed in path names a directory, or false
 * otherwise.
 */
bool
test_driver_is_directory(const char *path)
{
	struct stat sb;
	if (stat(path, &sb) != 0)
		return false;
	return S_ISDIR(sb.st_mode);
}
