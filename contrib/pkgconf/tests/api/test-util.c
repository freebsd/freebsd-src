/*
 * test-util.c
 * Tests for spdxtool's util functions especially URL/URI encoding.
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

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include "test-api.h"
#include "util.h"

static pkgconf_client_t *g_client;

static void
util_setup(void)
{
	g_client = test_client_new();
	// Mirror cli/spdxtool/main.c: the constructors read these globals
	spdxtool_util_set_uri_root(g_client, "https://example.com/test");
	spdxtool_util_set_uri_separator_colon(g_client, false);
	spdxtool_util_set_spdx_version(g_client, "3.0.1");
	spdxtool_util_set_spdx_license(g_client, "CC0-1.0");
}

// Test set and get_uri functions
static void
test_util_set_get_uri(void)
{
	TEST_ASSERT_STRCMP_EQ(spdxtool_util_get_uri_root(g_client), "https://example.com/test");
	spdxtool_util_set_uri_root(g_client, "https://example.com/test space");
	TEST_ASSERT_STRCMP_EQ(spdxtool_util_get_uri_root(g_client), "https://example.com/test space");
}

// Test that uri's get escaped
static void
test_util_id_string(void)
{
	char *id_str = spdxtool_util_get_spdx_id_string(g_client, "test", "test1");
	TEST_ASSERT_STRCMP_EQ(id_str, "https://example.com/test%20space/test/test1");
	free(id_str);
	id_str = spdxtool_util_get_spdx_id_string(g_client, "test^", "\\<text>%{another text}=+\"`|");
	TEST_ASSERT_STRCMP_EQ(id_str, "https://example.com/test%20space/test%5E/%5C%3Ctext%3E%25%7Banother%20text%7D=%2B%22%60%7C");
	free(id_str);
}

// Test that uri's get escaped
static void
test_util_id_string_urn(void)
{
	char *id_str = NULL;
	spdxtool_util_set_uri_root(g_client, "urn:example:com");
	spdxtool_util_set_uri_separator_colon(g_client, ":");

	id_str = spdxtool_util_get_spdx_id_string(g_client, "test part", "test1");
	TEST_ASSERT_STRCMP_EQ(id_str, "urn:example:com:test%20part:test1");
	free(id_str);

	id_str = spdxtool_util_get_spdx_id_string(g_client, "test part", "\\<text>%{another text}=+\"`|,'");
	TEST_ASSERT_STRCMP_EQ(id_str, "urn:example:com:test%20part:%5C%3Ctext%3E%25%7Banother%20text%7D=%2B%22%60%7C%2C%27");
	free(id_str);
}


// Test that num uri's also get escaped
static void
test_util_id_int(void)
{
	char *id_str = spdxtool_util_get_spdx_id_int(g_client, "test");
	TEST_ASSERT_STRCMP_EQ(id_str, "https://example.com/test%20space/test/1");
	free(id_str);
	id_str = spdxtool_util_get_spdx_id_int(g_client, "test\\^");
	TEST_ASSERT_STRCMP_EQ(id_str, "https://example.com/test%20space/test%5C%5E/2");
	free(id_str);
}

// Test that num uri's also get escaped
static void
test_util_id_int_urn(void)
{
	char *id_str = spdxtool_util_get_spdx_id_int(g_client, "test space");
	TEST_ASSERT_STRCMP_EQ(id_str, "urn:example:com:test%20space:3");
	free(id_str);
	id_str = spdxtool_util_get_spdx_id_int(g_client, "test\\^");
	TEST_ASSERT_STRCMP_EQ(id_str, "urn:example:com:test%5C%5E:4");
	free(id_str);
}


int
main(int argc, const char **argv)
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);
	util_setup();

	TEST_RUN(basename, test_util_set_get_uri);
	TEST_RUN(basename, test_util_id_string);
	TEST_RUN(basename, test_util_id_int);
	TEST_RUN(basename, test_util_id_string_urn);
	TEST_RUN(basename, test_util_id_int_urn);

	pkgconf_client_free(g_client);

	return EXIT_SUCCESS;
}
