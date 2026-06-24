/*
 * test-path-utils.c
 * Tests for libpkgconf internal path utility functions.
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2025-2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include <libpkgconf/path.h>
#include "test-api.h"

static void
test_path_find_basename(void)
{
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename("/usr/lib/pkgconfig/foo.pc"), "foo.pc");
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename("/usr/lib/pkgconfig"), "pkgconfig");
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename("foo.pc"), "foo.pc");
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename("/foo.pc"), "foo.pc");
	TEST_ASSERT_EMPTY_STRING(pkgconf_path_find_basename("/"));
	TEST_ASSERT_EMPTY_STRING(pkgconf_path_find_basename(""));
	TEST_ASSERT_EMPTY_STRING(pkgconf_path_find_basename("/usr/"));
	TEST_ASSERT_EMPTY_STRING(pkgconf_path_find_basename("usr/"));
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename("///usr/lib///pkgconfig///foo.pc"), "foo.pc");
#ifdef _WIN32
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename("C:\\lib\\pkgconfig\\foo.pc"), "foo.pc");
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename("C:/lib/pkgconfig/foo.pc"), "foo.pc");
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename("C:/lib\\pkgconfig/foo.pc"), "foo.pc");
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename("C:\\lib/pkgconfig\\foo.pc"), "foo.pc");
#endif
}

static void
test_path_trim_basename(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&buf, "/usr/lib/pkgconfig/foo.pc");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "/usr/lib/pkgconfig");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "/usr/lib");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "/usr");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "");
	TEST_ASSERT_FALSE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "");

	pkgconf_buffer_reset(&buf);
	pkgconf_buffer_append(&buf, "foo.pc");
	TEST_ASSERT_FALSE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "foo.pc");

	pkgconf_buffer_finalize(&buf);
}

static void
test_determine_prefix_logic(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	// Normal case
	pkgconf_buffer_append(&buf, "/opt/foo/lib/pkgconfig/bar.pc");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename(pkgconf_buffer_str(&buf)), "pkgconfig");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "/opt/foo");

	// Short path: /pkgconfig/foo.pc
	pkgconf_buffer_reset(&buf);
	pkgconf_buffer_append(&buf, "/pkgconfig/foo.pc");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename(pkgconf_buffer_str(&buf)), "pkgconfig");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf)); // trims pkgconfig, returns true because of /
	TEST_ASSERT_FALSE(pkgconf_path_trim_basename(&buf)); // fails to trim further

	// Another short path: lib/pkgconfig/foo.pc
	pkgconf_buffer_reset(&buf);
	pkgconf_buffer_append(&buf, "lib/pkgconfig/foo.pc");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_path_find_basename(pkgconf_buffer_str(&buf)), "pkgconfig");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_FALSE(pkgconf_path_trim_basename(&buf));

	// Trailing slash
	pkgconf_buffer_reset(&buf);
	pkgconf_buffer_append(&buf, "/usr/lib/pkgconfig/");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "/usr/lib/pkgconfig");
	TEST_ASSERT_TRUE(pkgconf_path_trim_basename(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "/usr/lib");

	pkgconf_buffer_finalize(&buf);
}

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_path_find_basename);
	TEST_RUN(basename, test_path_trim_basename);
	TEST_RUN(basename, test_determine_prefix_logic);

	return EXIT_SUCCESS;
}
