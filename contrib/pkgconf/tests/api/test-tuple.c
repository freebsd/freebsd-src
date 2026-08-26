/*
 * test-tuple.c
 * Tests for the public libpkgconf tuple API.
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2025 pkgconf authors (see AUTHORS).
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

static void
test_tuple_add_and_find(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t tuples = PKGCONF_LIST_INITIALIZER;

	/* parse=false means store the value verbatim, no bytecode compile.
	 * That's the right mode for storing already-evaluated values. */
	pkgconf_tuple_t *t = pkgconf_tuple_add(client, &tuples, "prefix", "/opt/foo", false, 0);
	TEST_ASSERT_NONNULL(t);

	const char *found = pkgconf_tuple_find(client, &tuples, "prefix");
	TEST_ASSERT_NONNULL(found);
	TEST_ASSERT_STRCMP_EQ(found, "/opt/foo");

	pkgconf_tuple_free(&tuples);
	pkgconf_client_free(client);
}

static void
test_tuple_find_absent(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t tuples = PKGCONF_LIST_INITIALIZER;

	const char *found = pkgconf_tuple_find(client, &tuples, "nonexistent");
	TEST_ASSERT_EMPTY_STRING(found);

	pkgconf_tuple_free(&tuples);
	pkgconf_client_free(client);
}

static void
test_tuple_add_multiple(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t tuples = PKGCONF_LIST_INITIALIZER;

	pkgconf_tuple_add(client, &tuples, "prefix", "/usr", false, 0);
	pkgconf_tuple_add(client, &tuples, "exec_prefix", "/usr/local", false, 0);
	pkgconf_tuple_add(client, &tuples, "libdir", "/usr/lib", false, 0);

	TEST_ASSERT_STRCMP_EQ(pkgconf_tuple_find(client, &tuples, "prefix"), "/usr");
	TEST_ASSERT_STRCMP_EQ(pkgconf_tuple_find(client, &tuples, "exec_prefix"), "/usr/local");
	TEST_ASSERT_STRCMP_EQ(pkgconf_tuple_find(client, &tuples, "libdir"), "/usr/lib");
	TEST_ASSERT_EMPTY_STRING(pkgconf_tuple_find(client, &tuples, "datadir"));

	pkgconf_tuple_free(&tuples);
	pkgconf_client_free(client);
}

static void
test_tuple_global_add_and_find(void)
{
	pkgconf_client_t *client = test_client_new();

	pkgconf_tuple_add_global(client, "PKG_TEST_KEY", "test_value");

	const char *found = pkgconf_tuple_find_global(client, "PKG_TEST_KEY");
	TEST_ASSERT_NONNULL(found);
	TEST_ASSERT_STRCMP_EQ(found, "test_value");

	pkgconf_tuple_free_global(client);
	pkgconf_client_free(client);
}

static void
test_tuple_global_find_absent(void)
{
	pkgconf_client_t *client = test_client_new();

	const char *found = pkgconf_tuple_find_global(client, "DOES_NOT_EXIST");
	TEST_ASSERT_EMPTY_STRING(found);

	pkgconf_client_free(client);
}

static void
test_tuple_define_global_kv_form(void)
{
	pkgconf_client_t *client = test_client_new();

	/* This is the exact path --define-variable=KEY=VALUE takes
	 * pkgconf_tuple_define_global parses the "key=value" form and inserts it as a global tuple. */
	pkgconf_tuple_define_global(client, "myvar=myvalue");

	const char *found = pkgconf_tuple_find_global(client, "myvar");
	TEST_ASSERT_NONNULL(found);
	TEST_ASSERT_STRCMP_EQ(found, "myvalue");

	pkgconf_tuple_free_global(client);
	pkgconf_client_free(client);
}

static void
test_tuple_define_global_with_equals_in_value(void)
{
	pkgconf_client_t *client = test_client_new();

	// Only the first '=' should split key from value; subsequent '=' characters belong to the value.
	pkgconf_tuple_define_global(client, "CFLAGS=-DFOO=bar");

	const char *found = pkgconf_tuple_find_global(client, "CFLAGS");
	TEST_ASSERT_NONNULL(found);
	TEST_ASSERT_STRCMP_EQ(found, "-DFOO=bar");

	pkgconf_tuple_free_global(client);
	pkgconf_client_free(client);
}

static void
test_tuple_global_multiple(void)
{
	pkgconf_client_t *client = test_client_new();

	pkgconf_tuple_define_global(client, "a=1");
	pkgconf_tuple_define_global(client, "b=2");
	pkgconf_tuple_define_global(client, "c=3");

	TEST_ASSERT_STRCMP_EQ(pkgconf_tuple_find_global(client, "a"), "1");
	TEST_ASSERT_STRCMP_EQ(pkgconf_tuple_find_global(client, "b"), "2");
	TEST_ASSERT_STRCMP_EQ(pkgconf_tuple_find_global(client, "c"), "3");

	pkgconf_tuple_free_global(client);
	pkgconf_client_free(client);
}

static void
test_tuple_global_free_resets(void)
{
	pkgconf_client_t *client = test_client_new();

	pkgconf_tuple_define_global(client, "temp=value");
	TEST_ASSERT_NONNULL(pkgconf_tuple_find_global(client, "temp"));

	pkgconf_tuple_free_global(client);
	TEST_ASSERT_EMPTY_STRING(pkgconf_tuple_find_global(client, "temp"));

	pkgconf_client_free(client);
}

static void
test_tuple_free_empty(void)
{
	pkgconf_list_t tuples = PKGCONF_LIST_INITIALIZER;

	// Freeing an empty list should be a no-op. Mostly an ASan/leak check smoke test.
	pkgconf_tuple_free(&tuples);
}

static void
test_tuple_define_variable_end_to_end(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;
 
	pkgconf_tuple_define_global(client, "myprefix=/opt/custom");
 
	char *out = pkgconf_bytecode_eval_str(client, &vars, "-I${myprefix}/include", &saw_sysroot);
 
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "-I/opt/custom/include");
 
	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_tuple_free_global(client);
	pkgconf_client_free(client);
}
 
static void
test_tuple_define_variable_overrides_local(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;
 
	pkgconf_variable_t *v = pkgconf_variable_get_or_create(&vars, "prefix");
	TEST_ASSERT_NONNULL(v);
	pkgconf_buffer_reset(&v->bcbuf);
	pkgconf_bytecode_compile(&v->bcbuf, "/usr");
	pkgconf_bytecode_from_buffer(&v->bc, &v->bcbuf);
 
	// Simulate user passing --define-variable=prefix=/custom.
	pkgconf_tuple_define_global(client, "prefix=/custom");
 
	char *out = pkgconf_bytecode_eval_str(client, &vars, "${prefix}/lib", &saw_sysroot);
 
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "/custom/lib");
 
	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_tuple_free_global(client);
	pkgconf_client_free(client);
}

static void
test_tuple_escaped_quote(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t tuples = PKGCONF_LIST_INITIALIZER;

	// A double-quoted value containing an escaped double-quote
	pkgconf_tuple_t *t = pkgconf_tuple_add(client, &tuples, "key", "\"a\\\"b\"", true, 0);
	TEST_ASSERT_NONNULL(t);

	const char *found = pkgconf_tuple_find(client, &tuples, "key");
	TEST_ASSERT_NONNULL(found);
	TEST_ASSERT_STRCMP_EQ(found, "a\"b");

	pkgconf_tuple_free(&tuples);
	pkgconf_client_free(client);
}

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_tuple_add_and_find);
	TEST_RUN(basename, test_tuple_find_absent);
	TEST_RUN(basename, test_tuple_add_multiple);
	TEST_RUN(basename, test_tuple_free_empty);
	TEST_RUN(basename, test_tuple_global_add_and_find);
	TEST_RUN(basename, test_tuple_global_find_absent);
	TEST_RUN(basename, test_tuple_define_global_kv_form);
	TEST_RUN(basename, test_tuple_define_global_with_equals_in_value);
	TEST_RUN(basename, test_tuple_global_multiple);
	TEST_RUN(basename, test_tuple_global_free_resets);
	TEST_RUN(basename, test_tuple_define_variable_end_to_end);
	TEST_RUN(basename, test_tuple_define_variable_overrides_local);
	TEST_RUN(basename, test_tuple_escaped_quote);

	return EXIT_SUCCESS;
}
