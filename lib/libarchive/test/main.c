/*
 * Copyright (c) 2003-2006 Tim Kientzle
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
#include <stdarg.h>
#include <time.h>

#include "test.h"
__FBSDID("$FreeBSD$");

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

void
failure(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsprintf(msg, fmt, ap);
	va_end(ap);
}

void
test_assert(const char *file, int line, int value, const char *condition, struct archive *a)
{
	if (value) {
		msg[0] = '\0';
		return;
	}
	fprintf(stderr, "%s:%d: Assertion failed\n", file, line);
	fprintf(stderr, "   Condition: %s\n", condition);
	if (msg[0] != '\0') {
		fprintf(stderr, "   Description: %s\n", msg);
		msg[0] = '\0';
	}
	if (a != NULL) {
		fprintf(stderr, "   archive error: %s\n", archive_error_string(a));
	}
	*(char *)(NULL) = 0;
	exit(1);
}

void
test_assert_equal_int(const char *file, int line,
    int v1, const char *e1, int v2, const char *e2, struct archive *a)
{
	if (v1 == v2) {
		msg[0] = '\0';
		return;
	}
	fprintf(stderr, "%s:%d: Assertion failed\n", file, line);
	fprintf(stderr, "   Condition: %s==%s\n", e1, e2);
	fprintf(stderr, "              %s=%d\n", e1, v1);
	fprintf(stderr, "              %s=%d\n", e2, v2);
	if (msg[0] != '\0') {
		fprintf(stderr, "   Description: %s\n", msg);
		msg[0] = '\0';
	}
	if (a != NULL) {
		fprintf(stderr, "   archive error: %s\n", archive_error_string(a));
	}
	*(char *)(NULL) = 0;
	exit(1);
}

/*
 * "list.h" is automatically generated; it just has a lot of lines like:
 * 	DEFINE_TEST(function_name)
 * The common "test.h" includes it to declare all of the test functions.
 * We reuse it here to define a list of all tests to run.
 */
#undef DEFINE_TEST
#define DEFINE_TEST(n) n, #n,
struct { void (*func)(void); char *name; } tests[] = {
	#include "list.h"
};

int main(int argc, char **argv)
{
	void (*f)(void);
	int limit = sizeof(tests) / sizeof(tests[0]);
	int i;
	time_t now;
	char tmpdir[256];
	int tmpdirHandle;

	/*
	 * Create a temp directory for the following tests.
	 * Include the time the tests started as part of the name,
	 * to make it easier to track the results of multiple tests.
	 */
	now = time(NULL);
	strftime(tmpdir, sizeof(tmpdir),
	    "/tmp/libarchive_test.%Y-%m-%dT%H.%M.%S",
	    localtime(&now));
	if (mkdir(tmpdir,0755) != 0) {
		fprintf(stderr, "ERROR: Unable to create temp directory %s\n",
		    tmpdir);
		exit(1);
	}

	printf("Running libarchive tests in: %s\n", tmpdir);

	for (i = 0; i < limit; i++) {
		printf("%d: %s\n", i, tests[i].name);
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
	}
	printf("%d tests succeeded.\n", limit);
	return (0);
}
