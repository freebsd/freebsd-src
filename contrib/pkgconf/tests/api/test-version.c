/*
 * test-version.c
 * Tests for the public libpkgconf version comparison API.
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "test-api.h"

static void
cmp_lt(const char *a, const char *b)
{
	TEST_ASSERT_EQ(pkgconf_compare_version(a, b), -1);
	TEST_ASSERT_EQ(pkgconf_compare_version(b, a), 1);
}

static void
cmp_eq(const char *a, const char *b)
{
	TEST_ASSERT_EQ(pkgconf_compare_version(a, b), 0);
	TEST_ASSERT_EQ(pkgconf_compare_version(b, a), 0);
}

static void
test_version_equal(void)
{
	cmp_eq("1.0.0", "1.0.0");
	cmp_eq("", "");
	cmp_eq("abc", "abc");
	cmp_eq("1.0a", "1.0A");
	cmp_eq("RELEASE", "release");
}

static void
test_version_null(void)
{
	TEST_ASSERT_EQ(pkgconf_compare_version(NULL, NULL), -1);
	cmp_lt(NULL, "1.0");
	cmp_lt(NULL, "");
}

static void
test_version_numeric(void)
{
	cmp_lt("1.0.0", "1.0.1");
	cmp_lt("0.9", "1.0");
	cmp_lt("1.9.9", "2.0.0");
	cmp_lt("1.9", "1.10");
	cmp_lt("1.9", "1.100");
}

static void
test_version_leading_zeros(void)
{
	cmp_eq("1.0", "1.00");
	cmp_eq("1.7", "1.007");
	cmp_eq("01.02.03", "1.2.3");
	cmp_lt("1.0.1", "1.0.10");
}

static void
test_version_component_count(void)
{
	cmp_lt("1.0", "1.0.1");
	cmp_lt("1.0", "1.0.0");
	cmp_lt("1.2.3", "1.2.3.1");
}

static void
test_version_separators(void)
{
	cmp_eq("1.2.3", "1-2-3");
	cmp_eq("1.2.3", "1_2_3");
	cmp_eq("1.2.3", "1:2:3");
	cmp_eq("1.0", "1...0");
}

static void
test_version_tilde(void)
{
	cmp_lt("1.0~rc1", "1.0");
	cmp_lt("1.0~rc1", "1.0~rc2");
	cmp_lt("1.0~~", "1.0~");
	cmp_lt("1.0~1", "1.0.1");
	cmp_lt("1.0.0~beta", "1.0.0");
}

static void
test_version_alpha_numeric(void)
{
	cmp_lt("1.b", "1.5");
	cmp_lt("1.0.alpha", "1.0.beta");
	cmp_lt("1.0alpha", "1.0alphabeta");
	cmp_lt("1.0", "1.0pre");
}

static void
test_version_real_world(void)
{
	cmp_lt("2.5.1", "2.9.91");
	cmp_lt("2.9.91", "3.0.0");
	cmp_lt("1.0.0~beta", "1.0.0~rc1");
	cmp_lt("1.0.0~rc1", "1.0.0");
}

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_version_equal);
	TEST_RUN(basename, test_version_null);
	TEST_RUN(basename, test_version_numeric);
	TEST_RUN(basename, test_version_leading_zeros);
	TEST_RUN(basename, test_version_component_count);
	TEST_RUN(basename, test_version_separators);
	TEST_RUN(basename, test_version_tilde);
	TEST_RUN(basename, test_version_alpha_numeric);
	TEST_RUN(basename, test_version_real_world);

	return EXIT_SUCCESS;
}
