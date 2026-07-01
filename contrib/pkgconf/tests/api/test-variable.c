/*
 * test-variable.c
 * Tests for the public libpkgconf variable API.
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

/*
 * Install a value on a variable by compiling it as bytecode.
 * Mirrors what the parser does when it reads a .pc field.
 */
static void
seed_variable(pkgconf_list_t *vars, const char *key, const char *value)
{
	pkgconf_variable_t *v = pkgconf_variable_get_or_create(vars, key);
	TEST_ASSERT_NONNULL(v);

	pkgconf_buffer_reset(&v->bcbuf);
	pkgconf_bytecode_compile(&v->bcbuf, value);
	pkgconf_bytecode_from_buffer(&v->bc, &v->bcbuf);
}

static void
test_variable_new_and_free(void)
{
	pkgconf_variable_t *v = pkgconf_variable_new("prefix");

	TEST_ASSERT_NONNULL(v);
	TEST_ASSERT_STRCMP_EQ(v->key, "prefix");

	pkgconf_variable_free(v);
}

static void
test_variable_get_or_create_creates(void)
{
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_variable_t *v = pkgconf_variable_get_or_create(&vars, "prefix");
	TEST_ASSERT_NONNULL(v);
	TEST_ASSERT_STRCMP_EQ(v->key, "prefix");

	pkgconf_variable_list_free(&vars);
}

static void
test_variable_get_or_create_returns_existing(void)
{
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_variable_t *first = pkgconf_variable_get_or_create(&vars, "libdir");
	TEST_ASSERT_NONNULL(first);

	// A second call with the same key should return the same object, not create a duplicate.
	pkgconf_variable_t *second = pkgconf_variable_get_or_create(&vars, "libdir");
	TEST_ASSERT_NONNULL(second);
	TEST_ASSERT_EQ(first, second);

	pkgconf_variable_list_free(&vars);
}

static void
test_variable_find_present(void)
{
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_variable_t *created = pkgconf_variable_get_or_create(&vars, "prefix");
	TEST_ASSERT_NONNULL(created);

	pkgconf_variable_t *found = pkgconf_variable_find(&vars, "prefix");
	TEST_ASSERT_NONNULL(found);
	TEST_ASSERT_EQ(created, found);

	pkgconf_variable_list_free(&vars);
}

static void
test_variable_find_absent(void)
{
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_variable_t *found = pkgconf_variable_find(&vars, "nonexistent");
	TEST_ASSERT_NULL(found);

	pkgconf_variable_list_free(&vars);
}

static void
test_variable_find_among_many(void)
{
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_variable_get_or_create(&vars, "prefix");
	pkgconf_variable_get_or_create(&vars, "exec_prefix");
	pkgconf_variable_get_or_create(&vars, "libdir");
	pkgconf_variable_get_or_create(&vars, "includedir");

	TEST_ASSERT_NONNULL(pkgconf_variable_find(&vars, "prefix"));
	TEST_ASSERT_NONNULL(pkgconf_variable_find(&vars, "exec_prefix"));
	TEST_ASSERT_NONNULL(pkgconf_variable_find(&vars, "libdir"));
	TEST_ASSERT_NONNULL(pkgconf_variable_find(&vars, "includedir"));
	TEST_ASSERT_NULL(pkgconf_variable_find(&vars, "notpresent"));

	pkgconf_variable_list_free(&vars);
}

static void
test_variable_delete(void)
{
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_variable_t *v = pkgconf_variable_get_or_create(&vars, "prefix");
	TEST_ASSERT_NONNULL(v);
	TEST_ASSERT_NONNULL(pkgconf_variable_find(&vars, "prefix"));

	pkgconf_variable_delete(&vars, v);
	TEST_ASSERT_NULL(pkgconf_variable_find(&vars, "prefix"));

	pkgconf_variable_list_free(&vars);
}

static void
test_variable_eval_str_plain(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	seed_variable(&vars, "version", "1.2.3");

	pkgconf_variable_t *v = pkgconf_variable_find(&vars, "version");
	TEST_ASSERT_NONNULL(v);

	char *out = pkgconf_variable_eval_str(client, &vars, v, &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "1.2.3");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_variable_eval_str_with_reference(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	// Standard pkg-config layout: libdir is derived from prefix.
	seed_variable(&vars, "prefix", "/opt/foo");
	seed_variable(&vars, "libdir", "${prefix}/lib");

	pkgconf_variable_t *libdir = pkgconf_variable_find(&vars, "libdir");
	TEST_ASSERT_NONNULL(libdir);

	char *out = pkgconf_variable_eval_str(client, &vars, libdir, &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "/opt/foo/lib");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_variable_eval_str_chained(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;
	bool saw_sysroot = false;

	seed_variable(&vars, "prefix", "/usr/local");
	seed_variable(&vars, "exec_prefix", "${prefix}");
	seed_variable(&vars, "includedir", "${exec_prefix}/include");

	pkgconf_variable_t *includedir = pkgconf_variable_find(&vars, "includedir");
	TEST_ASSERT_NONNULL(includedir);

	char *out = pkgconf_variable_eval_str(client, &vars, includedir, &saw_sysroot);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "/usr/local/include");

	free(out);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_variable_list_free_handles_empty(void)
{
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	// Freeing an empty list should be a no-op and not crash. Mostly an ASan/leak smoke test.
	pkgconf_variable_list_free(&vars);
}

static void
test_variable_list_free_handles_many(void)
{
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	/* Add a mix of bare and value-carrying variables, then free the whole list at once.
	 * Mostly an ASan/leak smoke test. */
	pkgconf_variable_get_or_create(&vars, "a");
	seed_variable(&vars, "b", "/path/b");
	pkgconf_variable_get_or_create(&vars, "c");
	seed_variable(&vars, "d", "${b}/sub");

	pkgconf_variable_list_free(&vars);
}

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_variable_new_and_free);
	TEST_RUN(basename, test_variable_get_or_create_creates);
	TEST_RUN(basename, test_variable_get_or_create_returns_existing);
	TEST_RUN(basename, test_variable_find_present);
	TEST_RUN(basename, test_variable_find_absent);
	TEST_RUN(basename, test_variable_find_among_many);
	TEST_RUN(basename, test_variable_delete);
	TEST_RUN(basename, test_variable_eval_str_plain);
	TEST_RUN(basename, test_variable_eval_str_with_reference);
	TEST_RUN(basename, test_variable_eval_str_chained);
	TEST_RUN(basename, test_variable_list_free_handles_empty);
	TEST_RUN(basename, test_variable_list_free_handles_many);

	return EXIT_SUCCESS;
}
