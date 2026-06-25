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

static const char *
nth_path(const pkgconf_list_t *list, size_t idx)
{
	pkgconf_node_t *n;
	size_t i = 0;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, n)
	{
		const pkgconf_path_t *p = n->data;

		if (i++ == idx)
			return p->path;
	}

	return NULL;
}

static void
test_path_prepend(void)
{
	pkgconf_list_t list = PKGCONF_LIST_INITIALIZER;

	pkgconf_path_prepend("/a", &list, false);
	pkgconf_path_prepend("/b", &list, false);
	pkgconf_path_prepend("/c", &list, false);

	TEST_ASSERT_EQ(list.length, 3);
	TEST_ASSERT_STRCMP_EQ(nth_path(&list, 0), "/c");
	TEST_ASSERT_STRCMP_EQ(nth_path(&list, 1), "/b");
	TEST_ASSERT_STRCMP_EQ(nth_path(&list, 2), "/a");

	pkgconf_path_add("/d", &list, false);
	TEST_ASSERT_EQ(list.length, 4);
	TEST_ASSERT_STRCMP_EQ(nth_path(&list, 0), "/c");
	TEST_ASSERT_STRCMP_EQ(nth_path(&list, 3), "/d");

	pkgconf_path_prepend("/e//f", &list, false);
	TEST_ASSERT_STRCMP_EQ(nth_path(&list, 0), "/e/f");

	pkgconf_path_free(&list);
	TEST_ASSERT_EQ(list.length, 0);
	TEST_ASSERT_NULL(list.head);
	TEST_ASSERT_NULL(list.tail);
}

static void
test_path_prepend_filter(void)
{
	pkgconf_list_t list = PKGCONF_LIST_INITIALIZER;

	pkgconf_path_prepend(".", &list, true);
	TEST_ASSERT_EQ(list.length, 1);

	pkgconf_path_prepend(".", &list, true);
	TEST_ASSERT_EQ(list.length, 1);

	pkgconf_path_prepend("..", &list, true);
	TEST_ASSERT_EQ(list.length, 2);

	pkgconf_path_prepend(".", &list, false);
	TEST_ASSERT_EQ(list.length, 3);

	pkgconf_path_free(&list);
}

static void
test_path_prepend_list(void)
{
	pkgconf_list_t src = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t dst = PKGCONF_LIST_INITIALIZER;

	pkgconf_path_add("/s1", &src, false);
	pkgconf_path_add("/s2", &src, false);
	pkgconf_path_add("/s3", &src, false);

	pkgconf_path_add("/d1", &dst, false);
	pkgconf_path_add("/d2", &dst, false);

	pkgconf_path_prepend_list(&dst, &src);

	TEST_ASSERT_EQ(dst.length, 5);
	TEST_ASSERT_STRCMP_EQ(nth_path(&dst, 0), "/s3");
	TEST_ASSERT_STRCMP_EQ(nth_path(&dst, 1), "/s2");
	TEST_ASSERT_STRCMP_EQ(nth_path(&dst, 2), "/s1");
	TEST_ASSERT_STRCMP_EQ(nth_path(&dst, 3), "/d1");
	TEST_ASSERT_STRCMP_EQ(nth_path(&dst, 4), "/d2");

	TEST_ASSERT_EQ(src.length, 3);
	TEST_ASSERT_STRCMP_EQ(nth_path(&src, 0), "/s1");
	TEST_ASSERT_TRUE(nth_path(&dst, 2) != nth_path(&src, 0));

	pkgconf_path_free(&dst);
	TEST_ASSERT_EQ(dst.length, 0);

	TEST_ASSERT_EQ(src.length, 3);
	TEST_ASSERT_STRCMP_EQ(nth_path(&src, 2), "/s3");

	pkgconf_path_prepend_list(&dst, &src);
	TEST_ASSERT_EQ(dst.length, 3);
	TEST_ASSERT_STRCMP_EQ(nth_path(&dst, 0), "/s3");
	TEST_ASSERT_STRCMP_EQ(nth_path(&dst, 2), "/s1");

	pkgconf_path_free(&src);
	pkgconf_path_free(&dst);
}

static bool
plausible(const char *text)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	bool result;

	pkgconf_buffer_append(&buf, text);
	result = pkgconf_path_is_plausible(&buf);
	pkgconf_buffer_finalize(&buf);

	return result;
}

static void
test_path_is_plausible(void)
{
	pkgconf_buffer_t empty = PKGCONF_BUFFER_INITIALIZER;

	TEST_ASSERT_FALSE(pkgconf_path_is_plausible(NULL));
	TEST_ASSERT_FALSE(pkgconf_path_is_plausible(&empty));
	pkgconf_buffer_finalize(&empty);

	TEST_ASSERT_FALSE(plausible(""));
	TEST_ASSERT_FALSE(plausible("   "));
	TEST_ASSERT_FALSE(plausible("libfoo"));
	TEST_ASSERT_FALSE(plausible("foo bar"));
	TEST_ASSERT_FALSE(plausible("."));
	TEST_ASSERT_FALSE(plausible(".."));

	TEST_ASSERT_TRUE(plausible("/usr/lib/pkgconfig"));
	TEST_ASSERT_TRUE(plausible("  /usr/lib"));
	TEST_ASSERT_TRUE(plausible("./foo"));
	TEST_ASSERT_TRUE(plausible("../foo"));
	TEST_ASSERT_TRUE(plausible(".\\foo"));
	TEST_ASSERT_TRUE(plausible("..\\foo"));
	TEST_ASSERT_TRUE(plausible("C:/foo"));
	TEST_ASSERT_TRUE(plausible("C:\\foo"));
	TEST_ASSERT_TRUE(plausible("relative/path"));
	TEST_ASSERT_TRUE(plausible("relative\\path"));
	TEST_ASSERT_TRUE(plausible("Program Files/MySDK"));
}

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_path_find_basename);
	TEST_RUN(basename, test_path_trim_basename);
	TEST_RUN(basename, test_determine_prefix_logic);
	TEST_RUN(basename, test_path_prepend);
	TEST_RUN(basename, test_path_prepend_filter);
	TEST_RUN(basename, test_path_prepend_list);
	TEST_RUN(basename, test_path_is_plausible);

	return EXIT_SUCCESS;
}
