/*
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 * Various utility routines useful for test programs.
 * Each test program is linked against this file.
 */
#include <errno.h>
#include <stdarg.h>
#include <time.h>

#include "test.h"
__FBSDID("$FreeBSD$");

/* Interix doesn't define these in a standard header. */
#if __INTERIX__
extern char *optarg;
extern int optind;
#endif

/* Default is to crash and try to force a core dump on failure. */
static int dump_on_failure = 1;
/* Default is to print some basic information about each test. */
static int quiet_flag = 0;
/* Cumulative count of component failures. */
static int failures = 0;
/* Cumulative count of skipped component tests. */
static int skips = 0;

/*
 * My own implementation of the standard assert() macro emits the
 * message in the same format as GCC (file:line: message).
 * It also includes some additional useful information.
 * This makes it a lot easier to skim through test failures in
 * Emacs.  ;-)
 *
 * It also supports a few special features specifically to simplify
 * libarchive test harnesses:
 *    failure(fmt, args) -- Stores a text string that gets
 *          printed if the following assertion fails, good for
 *          explaining subtle tests.
 *    assertA(a, cond) -- If the test fails, also prints out any error
 *          message stored in archive object 'a'.
 */
static char msg[4096];

/*
 * For each test source file, we remember how many times each
 * failure was reported.
 */
static const char *failed_filename;
static struct line {
	int line;
	int count;
}  failed_lines[1000];


/* Count this failure; return the number of previous failures. */
static int
previous_failures(const char *filename, int line)
{
	int i;
	int count;

	if (failed_filename == NULL || strcmp(failed_filename, filename) != 0)
		memset(failed_lines, 0, sizeof(failed_lines));
	failed_filename = filename;

	for (i = 0; i < sizeof(failed_lines)/sizeof(failed_lines[0]); i++) {
		if (failed_lines[i].line == line) {
			count = failed_lines[i].count;
			failed_lines[i].count++;
			return (count);
		}
		if (failed_lines[i].line == 0) {
			failed_lines[i].line = line;
			failed_lines[i].count = 1;
			return (0);
		}
	}
}

/* Inform user that we're skipping a test. */
static const char *skipped_filename;
static int skipped_line;
void skipping_setup(const char *filename, int line)
{
	skipped_filename = filename;
	skipped_line = line;
}
void
test_skipping(const char *fmt, ...)
{
	int i;
	int line = skipped_line;
	va_list ap;

	if (previous_failures(skipped_filename, skipped_line))
		return;

	va_start(ap, fmt);
	fprintf(stderr, " *** SKIPPING: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	++skips;
}

/* Common handling of failed tests. */
static void
test_failed(struct archive *a, int line)
{
	int i;

	failures ++;

	if (msg[0] != '\0') {
		fprintf(stderr, "   Description: %s\n", msg);
		msg[0] = '\0';
	}
	if (a != NULL) {
		fprintf(stderr, "   archive error: %s\n", archive_error_string(a));
	}

	if (dump_on_failure) {
		fprintf(stderr, " *** forcing core dump so failure can be debugged ***\n");
		*(char *)(NULL) = 0;
		exit(1);
	}
}

/* Summarize repeated failures in the just-completed test file. */
int
summarize_comparator(const void *a0, const void *b0)
{
	const struct line *a = a0, *b = b0;
	if (a->line == 0 && b->line == 0)
		return (0);
	if (a->line == 0)
		return (1);
	if (b->line == 0)
		return (-1);
	return (a->line - b->line);
}

void
summarize(const char *filename)
{
	int i;

	qsort(failed_lines, sizeof(failed_lines)/sizeof(failed_lines[0]),
	    sizeof(failed_lines[0]), summarize_comparator);
	for (i = 0; i < sizeof(failed_lines)/sizeof(failed_lines[0]); i++) {
		if (failed_lines[i].line == 0)
			break;
		if (failed_lines[i].count > 1)
			fprintf(stderr, "%s:%d: Failed %d times\n",
			    failed_filename, failed_lines[i].line,
			    failed_lines[i].count);
	}
	/* Clear the failure history for the next file. */
	memset(failed_lines, 0, sizeof(failed_lines));
}

/* Set up a message to display only after a test fails. */
void
failure(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsprintf(msg, fmt, ap);
	va_end(ap);
}

/* Generic assert() just displays the failed condition. */
void
test_assert(const char *file, int line, int value, const char *condition, struct archive *a)
{
	if (value) {
		msg[0] = '\0';
		return;
	}
	if (previous_failures(file, line))
		return;
	fprintf(stderr, "%s:%d: Assertion failed\n", file, line);
	fprintf(stderr, "   Condition: %s\n", condition);
	test_failed(a, line);
}

/* assertEqualInt() displays the values of the two integers. */
void
test_assert_equal_int(const char *file, int line,
    int v1, const char *e1, int v2, const char *e2, struct archive *a)
{
	if (v1 == v2) {
		msg[0] = '\0';
		return;
	}
	if (previous_failures(file, line))
		return;
	fprintf(stderr, "%s:%d: Assertion failed: Ints not equal\n",
	    file, line);
	fprintf(stderr, "      %s=%d\n", e1, v1);
	fprintf(stderr, "      %s=%d\n", e2, v2);
	test_failed(a, line);
}

/* assertEqualString() displays the values of the two strings. */
void
test_assert_equal_string(const char *file, int line,
    const char *v1, const char *e1,
    const char *v2, const char *e2,
    struct archive *a)
{
	if (v1 == NULL || v2 == NULL) {
		if (v1 == v2) {
			msg[0] = '\0';
			return;
		}
	} else if (strcmp(v1, v2) == 0) {
		msg[0] = '\0';
		return;
	}
	if (previous_failures(file, line))
		return;
	fprintf(stderr, "%s:%d: Assertion failed: Strings not equal\n",
	    file, line);
	fprintf(stderr, "      %s = \"%s\"\n", e1, v1);
	fprintf(stderr, "      %s = \"%s\"\n", e2, v2);
	test_failed(a, line);
}

/* assertEqualWString() displays the values of the two strings. */
void
test_assert_equal_wstring(const char *file, int line,
    const wchar_t *v1, const char *e1,
    const wchar_t *v2, const char *e2,
    struct archive *a)
{
	if (wcscmp(v1, v2) == 0) {
		msg[0] = '\0';
		return;
	}
	if (previous_failures(file, line))
		return;
	fprintf(stderr, "%s:%d: Assertion failed: Unicode strings not equal\n",
	    file, line);
	fwprintf(stderr, L"      %s = \"%ls\"\n", e1, v1);
	fwprintf(stderr, L"      %s = \"%ls\"\n", e2, v2);
	test_failed(a, line);
}

/*
 * "list.h" is automatically generated; it just has a lot of lines like:
 * 	DEFINE_TEST(function_name)
 * The common "test.h" includes it to declare all of the test functions.
 * We reuse it here to define a list of all tests to run.
 */
#undef DEFINE_TEST
#define DEFINE_TEST(n) { n, #n },
struct { void (*func)(void); const char *name; } tests[] = {
	#include "list.h"
};

static int test_run(int i, const char *tmpdir)
{
	int failures_before = failures;

	if (!quiet_flag)
		printf("%d: %s\n", i, tests[i].name);
	/*
	 * Always explicitly chdir() in case the last test moved us to
	 * a strange place.
	 */
	if (chdir(tmpdir)) {
		fprintf(stderr,
		    "ERROR: Couldn't chdir to temp dir %s\n",
		    tmpdir);
		exit(1);
	}
	/* Create a temp directory for this specific test. */
	if (mkdir(tests[i].name, 0755)) {
		fprintf(stderr,
		    "ERROR: Couldn't create temp dir ``%s''\n",
		    tests[i].name);
		exit(1);
	}
	if (chdir(tests[i].name)) {
		fprintf(stderr,
		    "ERROR: Couldn't chdir to temp dir ``%s''\n",
		    tests[i].name);
		exit(1);
	}
	(*tests[i].func)();
	summarize(tests[i].name);
	return (failures == failures_before ? 0 : 1);
}

static void usage(void)
{
	static const int limit = sizeof(tests) / sizeof(tests[0]);
	int i;

	printf("Usage: libarchive_test [options] <test> <test> ...\n");
	printf("Default is to run all tests.\n");
	printf("Otherwise, specify the numbers of the tests you wish to run.\n");
	printf("Options:\n");
	printf("  -k  Keep running after failures.\n");
	printf("      Default: Core dump after any failure.\n");
	printf("  -q  Quiet.\n");
	printf("Available tests:\n");
	for (i = 0; i < limit; i++)
		printf("  %d: %s\n", i, tests[i].name);
	exit(1);
}

int main(int argc, char **argv)
{
	static const int limit = sizeof(tests) / sizeof(tests[0]);
	int i, tests_run = 0, tests_failed = 0, opt;
	time_t now;
	char tmpdir[256];

	while ((opt = getopt(argc, argv, "kq")) != -1) {
		switch (opt) {
		case 'k':
			dump_on_failure = 0;
			break;
		case 'q':
			quiet_flag = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/*
	 * Create a temp directory for the following tests.
	 * Include the time the tests started as part of the name,
	 * to make it easier to track the results of multiple tests.
	 */
	now = time(NULL);
	for (i = 0; i < 1000; i++) {
		strftime(tmpdir, sizeof(tmpdir),
		    "/tmp/libarchive_test.%Y-%m-%dT%H.%M.%S",
		    localtime(&now));
		sprintf(tmpdir + strlen(tmpdir), "-%03d", i);
		if (mkdir(tmpdir,0755) == 0)
			break;
		if (errno == EEXIST)
			continue;
		fprintf(stderr, "ERROR: Unable to create temp directory %s\n",
		    tmpdir);
		exit(1);
	}

	if (!quiet_flag) {
		printf("Running libarchive tests in: %s\n", tmpdir);
		printf("Exercising %s\n", archive_version());
	}

	if (argc == 0) {
		/* Default: Run all tests. */
		for (i = 0; i < limit; i++) {
			if (test_run(i, tmpdir))
				tests_failed++;
			tests_run++;
		}
	} else {
		while (*(argv) != NULL) {
			i = atoi(*argv);
			if (**argv < '0' || **argv > '9' || i < 0 || i >= limit) {
				printf("*** INVALID Test %s\n", *argv);
				usage();
			} else {
				if (test_run(i, tmpdir))
					tests_failed++;
				tests_run++;
			}
			argv++;
		}
	}
	printf("\n");
	printf("%d of %d test groups reported failures\n",
	    tests_failed, tests_run);
	printf(" Total of %d individual tests failed.\n", failures);
	printf(" Total of %d individual tests were skipped.\n", skips);
	return (tests_failed);
}
