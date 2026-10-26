/*-
 * Copyright (c) 2026 François Degros
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
#include "test.h"

/*
 * Regression test for the program filter (archive_read_support_filter_
 * program()) passing the correct environment to the filter program it
 * spawns. On POSIX systems, the filter program used to be spawned via
 * posix_spawnp() with a NULL envp, which starts the child with an empty
 * environment instead of inheriting ours (see filter_fork_posix.c). The
 * Windows implementation (filter_fork_windows.c) never had this bug: it
 * always passes a NULL lpEnvironment to CreateProcess(), which inherits
 * our environment as-is; so this is only tested on POSIX systems.
 *
 * This exercises that through the public program-filter API rather than
 * the low-level __archive_create_child() directly, using "env" as the
 * filter program: its output becomes the (single) raw-format entry's
 * content, and should include the marker variable we set beforehand.
 */

DEFINE_TEST(test_create_child_environment)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	skipping("The program filter's environment handling is only "
	    "tested on POSIX systems");
#else
	static const char varname[] = "LIBARCHIVE_TEST_CREATE_CHILD_ENV";
	static const char varvalue[] = "seen-by-child";
	static const char needle[] =
	    "LIBARCHIVE_TEST_CREATE_CHILD_ENV=seen-by-child";
	/* "env" ignores its input, but the program filter still needs
	 * something to feed it. */
	static const char input[] = { 0 };
	struct archive *a;
	struct archive_entry *ae;
	char buf[8192];
	size_t total;
	ssize_t n;
	int r;

	if (!canRunCommand("env", NULL)) {
		skipping("Can't run the \"env\" program on this platform");
		return;
	}

	/* Set a marker variable that only our own process has, so we
	 * can unambiguously tell whether the child inherited it. */
	assertEqualInt(0, setenv(varname, varvalue, 1));

	assert((a = archive_read_new()) != NULL);
	r = archive_read_support_filter_program(a, "env");
	if (r == ARCHIVE_FATAL) {
		archive_read_free(a);
		unsetenv(varname);
		skipping("archive_read_support_filter_program() "
		    "unsupported on this platform");
		return;
	}
	assertEqualIntA(a, ARCHIVE_OK, r);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, input, sizeof(input)));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	total = 0;
	while (total < sizeof(buf) - 1) {
		n = archive_read_data(a, buf + total, sizeof(buf) - 1 - total);
		if (n < 0)
			break;
		if (n == 0)
			break;
		total += (size_t)n;
	}
	buf[total] = '\0';

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	assertEqualInt(0, unsetenv(varname));

	/* The filter program's output should list our marker variable:
	 * if it doesn't, it was spawned with an empty (or at least
	 * incomplete) environment. */
	assert(strstr(buf, needle) != NULL);
#endif
}
