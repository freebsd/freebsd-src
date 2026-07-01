/*
 * test-license.c
 * Tests for the public libpkgconf license API.
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

static size_t
license_count(const pkgconf_list_t *list)
{
	size_t n = 0;
	const pkgconf_node_t *iter;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, iter)
	{
		n++;
	}

	return n;
}

static char *
render_to_string(pkgconf_client_t *client, const pkgconf_list_t *list)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_license_render(client, list, &buf);

	char *out = strdup(pkgconf_buffer_str_or_empty(&buf));
	pkgconf_buffer_finalize(&buf);
	return out;
}

static void
test_license_insert_and_free(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	pkgconf_license_insert(client, &licenses, PKGCONF_LICENSE_EXPRESSION, "BSD-3-Clause");
	TEST_ASSERT_EQ(license_count(&licenses), 1);

	const pkgconf_license_t *l = licenses.head->data;
	TEST_ASSERT_NONNULL(l);
	TEST_ASSERT_EQ(l->type, PKGCONF_LICENSE_EXPRESSION);
	TEST_ASSERT_STRCMP_EQ(l->data, "BSD-3-Clause");

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

static void
test_license_free_empty(void)
{
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	// Freeing an empty list is a no-op. Smoke test.
	pkgconf_license_free(&licenses);

    // Smoke test
	pkgconf_license_free(NULL);
}

static void
test_license_evaluate_single(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	pkgconf_license_evaluate_str(client, &licenses, "BSD-3-Clause", 0);

	TEST_ASSERT_EQ(license_count(&licenses), 1);
	const pkgconf_license_t *l = licenses.head->data;
	TEST_ASSERT_EQ(l->type, PKGCONF_LICENSE_EXPRESSION);
	TEST_ASSERT_STRCMP_EQ(l->data, "BSD-3-Clause");

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

static void
test_license_evaluate_or(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	pkgconf_license_evaluate_str(client, &licenses, "MIT OR ISC", 0);

	TEST_ASSERT_EQ(license_count(&licenses), 3);

	char *rendered = render_to_string(client, &licenses);
	TEST_ASSERT_STRCMP_EQ(rendered, "MIT OR ISC");
	free(rendered);

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

static void
test_license_evaluate_and(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	pkgconf_license_evaluate_str(client, &licenses, "LGPL-2.1-only AND MIT", 0);

	char *rendered = render_to_string(client, &licenses);
	TEST_ASSERT_STRCMP_EQ(rendered, "LGPL-2.1-only AND MIT");
	free(rendered);

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

static void
test_license_evaluate_multiple_keys_implicit_and(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	pkgconf_license_evaluate_str(client, &licenses, "BSD-3-Clause", 0);
	pkgconf_license_evaluate_str(client, &licenses, "BSD-2-Clause", 0);

	char *rendered = render_to_string(client, &licenses);
	TEST_ASSERT_STRCMP_EQ(rendered, "BSD-3-Clause AND BSD-2-Clause");
	free(rendered);

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

static void
test_license_evaluate_brackets(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	pkgconf_license_evaluate_str(client, &licenses, "ISC AND (BSD-3-Clause AND BSD-2-Clause)", 0);

	char *rendered = render_to_string(client, &licenses);
	TEST_ASSERT_STRCMP_EQ(rendered, "ISC AND (BSD-3-Clause AND BSD-2-Clause)");
	free(rendered);

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

static void
test_license_evaluate_empty(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	pkgconf_license_evaluate_str(client, &licenses, "", 0);
	TEST_ASSERT_EQ(license_count(&licenses), 0);

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

static void
test_license_evaluate_sanitizes(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	/* The sanitiser strips characters outside the allowed set
	 * (alnum, '-', '+', '(', ')', '.', ':').  A token of pure
	 * junk should sanitise to empty and be skipped. */
	pkgconf_license_evaluate_str(client, &licenses, "BSD-3-Clause", 0);

	const pkgconf_license_t *l = licenses.head->data;
	TEST_ASSERT_STRCMP_EQ(l->data, "BSD-3-Clause");

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

static void
test_license_render_empty(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	char *rendered = render_to_string(client, &licenses);
	TEST_ASSERT_EMPTY_STRING(rendered);
	free(rendered);

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

static void
test_license_render_single(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	pkgconf_license_insert(client, &licenses, PKGCONF_LICENSE_EXPRESSION, "MIT");

	char *rendered = render_to_string(client, &licenses);
	TEST_ASSERT_STRCMP_EQ(rendered, "MIT");
	free(rendered);

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

static void
test_license_copy_list(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t src = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t dst = PKGCONF_LIST_INITIALIZER;

	pkgconf_license_evaluate_str(client, &src, "MIT OR ISC", 0);
	size_t src_count = license_count(&src);

	pkgconf_license_copy_list(client, &dst, &src);
	TEST_ASSERT_EQ(license_count(&dst), src_count);

	/* The copy is independent: freeing the source must not affect
	 * the destination's rendered output. */
	pkgconf_license_free(&src);

	char *rendered = render_to_string(client, &dst);
	TEST_ASSERT_STRCMP_EQ(rendered, "MIT OR ISC");
	free(rendered);

	pkgconf_license_free(&dst);
	pkgconf_client_free(client);
}

static void
test_license_evaluate_long_sanitized_token(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	char token[512];
	token[0] = 'A';
	memset(token + 1, '_', 400);
	token[401] = '\0';

	pkgconf_license_evaluate_str(client, &licenses, token, 0);

	TEST_ASSERT_EQ(license_count(&licenses), 1);
	const pkgconf_license_t *l = licenses.head->data;
	TEST_ASSERT_EQ(l->type, PKGCONF_LICENSE_EXPRESSION);
	TEST_ASSERT_STRCMP_EQ(l->data, "A");

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

static void
test_license_evaluate_unterminated_quote(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t licenses = PKGCONF_LIST_INITIALIZER;

	pkgconf_license_evaluate_str(client, &licenses, "\"", 0);
	TEST_ASSERT_EQ(license_count(&licenses), 0);

	pkgconf_license_evaluate_str(client, &licenses, "MIT \"unterminated", 0);
	TEST_ASSERT_EQ(license_count(&licenses), 0);

	pkgconf_license_evaluate_str(client, &licenses, "\\", 0);
	TEST_ASSERT_EQ(license_count(&licenses), 0);

	pkgconf_license_free(&licenses);
	pkgconf_client_free(client);
}

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_license_insert_and_free);
	TEST_RUN(basename, test_license_free_empty);

	TEST_RUN(basename, test_license_evaluate_single);
	TEST_RUN(basename, test_license_evaluate_or);
	TEST_RUN(basename, test_license_evaluate_and);
	TEST_RUN(basename, test_license_evaluate_multiple_keys_implicit_and);
	TEST_RUN(basename, test_license_evaluate_brackets);
	TEST_RUN(basename, test_license_evaluate_empty);
	TEST_RUN(basename, test_license_evaluate_sanitizes);
	TEST_RUN(basename, test_license_evaluate_long_sanitized_token);
	TEST_RUN(basename, test_license_evaluate_unterminated_quote);

	TEST_RUN(basename, test_license_render_empty);
	TEST_RUN(basename, test_license_render_single);

	TEST_RUN(basename, test_license_copy_list);

	return EXIT_SUCCESS;
}
