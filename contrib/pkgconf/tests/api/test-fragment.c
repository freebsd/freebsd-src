/*
 * test-fragment.c
 * Tests for the public libpkgconf fragment API.
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
fragment_count(const pkgconf_list_t *list)
{
	size_t n = 0;
	const pkgconf_node_t *iter;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, iter)
	{
		n++;
	}

	return n;
}

static const pkgconf_fragment_t *
fragment_at(const pkgconf_list_t *list, size_t index)
{
	const pkgconf_node_t *iter;
	size_t i = 0;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, iter)
	{
		if (i++ == index)
			return iter->data;
	}

	return NULL;
}

/*
 * Render a fragment list to a newly-allocated C string for assertions.
 * Caller frees.
 */
static char *
render_to_string(const pkgconf_list_t *list)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_fragment_render_buf(list, &buf, false, NULL, ' ');

	if (pkgconf_buffer_str(&buf) == NULL)
		return strdup("");

	char *out = strdup(pkgconf_buffer_str(&buf));
	pkgconf_buffer_finalize(&buf);
	return out;
}

static void
test_fragment_parse_cflags(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t frags = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	TEST_ASSERT_TRUE(pkgconf_fragment_parse(client, &frags, &vars, "-I/usr/include -DFOO=1", 0));
	TEST_ASSERT_EQ(fragment_count(&frags), 2);

	const pkgconf_fragment_t *first = fragment_at(&frags, 0);
	TEST_ASSERT_NONNULL(first);
	TEST_ASSERT_EQ(first->type, 'I');
	TEST_ASSERT_STRCMP_EQ(first->data, "/usr/include");

	pkgconf_fragment_free(&frags);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_fragment_parse_libs(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t frags = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	TEST_ASSERT_TRUE(pkgconf_fragment_parse(client, &frags, &vars, "-L/usr/lib -lfoo -lbar", 0));
	TEST_ASSERT_EQ(fragment_count(&frags), 3);

	const pkgconf_fragment_t *f0 = fragment_at(&frags, 0);
	const pkgconf_fragment_t *f1 = fragment_at(&frags, 1);
	const pkgconf_fragment_t *f2 = fragment_at(&frags, 2);

	TEST_ASSERT_NONNULL(f0);
	TEST_ASSERT_NONNULL(f1);
	TEST_ASSERT_NONNULL(f2);

	TEST_ASSERT_EQ(f0->type, 'L');
	TEST_ASSERT_STRCMP_EQ(f0->data, "/usr/lib");
	TEST_ASSERT_EQ(f1->type, 'l');
	TEST_ASSERT_STRCMP_EQ(f1->data, "foo");
	TEST_ASSERT_EQ(f2->type, 'l');
	TEST_ASSERT_STRCMP_EQ(f2->data, "bar");

	pkgconf_fragment_free(&frags);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_fragment_parse_empty(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t frags = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	TEST_ASSERT_TRUE(pkgconf_fragment_parse(client, &frags, &vars, "", 0));
	TEST_ASSERT_EQ(fragment_count(&frags), 0);

	pkgconf_fragment_free(&frags);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_fragment_add_single(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t frags = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_fragment_add(client, &frags, &vars, "-I/opt/include", 0);

	TEST_ASSERT_EQ(fragment_count(&frags), 1);

	const pkgconf_fragment_t *f = fragment_at(&frags, 0);
	TEST_ASSERT_NONNULL(f);
	TEST_ASSERT_EQ(f->type, 'I');
	TEST_ASSERT_STRCMP_EQ(f->data, "/opt/include");

	pkgconf_fragment_free(&frags);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_fragment_render_cflags(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t frags = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_fragment_parse(client, &frags, &vars, "-I/usr/include -I/opt/include", 0);

	char *rendered = render_to_string(&frags);
	TEST_ASSERT_NONNULL(rendered);
	TEST_ASSERT_STRCMP_EQ(rendered, "-I/usr/include -I/opt/include");

	free(rendered);
	pkgconf_fragment_free(&frags);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_fragment_render_libs(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t frags = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_fragment_parse(client, &frags, &vars, "-L/usr/lib -lfoo", 0);

	char *rendered = render_to_string(&frags);
	TEST_ASSERT_NONNULL(rendered);
	TEST_ASSERT_STRCMP_EQ(rendered, "-L/usr/lib -lfoo");

	free(rendered);
	pkgconf_fragment_free(&frags);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_fragment_render_empty(void)
{
	pkgconf_list_t frags = PKGCONF_LIST_INITIALIZER;

	char *rendered = render_to_string(&frags);
	TEST_ASSERT_NONNULL(rendered);
	TEST_ASSERT_EMPTY_STRING(rendered);

	free(rendered);
}

// Filter predicate: keep only -I (include) fragments.
static bool
filter_only_includes(const pkgconf_client_t *client, const pkgconf_fragment_t *frag, void *data)
{
	(void) client;
	(void) data;
	return frag->type == 'I';
}

// Filter predicate: keep only -l (library name) fragments.
static bool
filter_only_libnames(const pkgconf_client_t *client, const pkgconf_fragment_t *frag, void *data)
{
	(void) client;
	(void) data;
	return frag->type == 'l';
}

// Filter predicate: keep nothing.
static bool
filter_nothing(const pkgconf_client_t *client, const pkgconf_fragment_t *frag, void *data)
{
	(void) client;
	(void) frag;
	(void) data;
	return false;
}

static void
test_fragment_filter_only_includes(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t src = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t dst = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_fragment_parse(client, &src, &vars, "-I/usr/include -L/usr/lib -lfoo -I/opt/include", 0);

	pkgconf_fragment_filter(client, &dst, &src, filter_only_includes, NULL);
	TEST_ASSERT_EQ(fragment_count(&dst), 2);

	const pkgconf_fragment_t *f0 = fragment_at(&dst, 0);
	const pkgconf_fragment_t *f1 = fragment_at(&dst, 1);
	TEST_ASSERT_NONNULL(f0);
	TEST_ASSERT_NONNULL(f1);
	TEST_ASSERT_EQ(f0->type, 'I');
	TEST_ASSERT_STRCMP_EQ(f0->data, "/usr/include");
	TEST_ASSERT_EQ(f1->type, 'I');
	TEST_ASSERT_STRCMP_EQ(f1->data, "/opt/include");

	pkgconf_fragment_free(&dst);
	pkgconf_fragment_free(&src);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_fragment_filter_only_libnames(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t src = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t dst = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_fragment_parse(client, &src, &vars, "-L/usr/lib -lfoo -lbar -I/usr/include", 0);

	pkgconf_fragment_filter(client, &dst, &src, filter_only_libnames, NULL);

	TEST_ASSERT_EQ(fragment_count(&dst), 2);

	const pkgconf_fragment_t *f0 = fragment_at(&dst, 0);
	const pkgconf_fragment_t *f1 = fragment_at(&dst, 1);
	TEST_ASSERT_NONNULL(f0);
	TEST_ASSERT_NONNULL(f1);
	TEST_ASSERT_EQ(f0->type, 'l');
	TEST_ASSERT_STRCMP_EQ(f0->data, "foo");
	TEST_ASSERT_EQ(f1->type, 'l');
	TEST_ASSERT_STRCMP_EQ(f1->data, "bar");

	pkgconf_fragment_free(&dst);
	pkgconf_fragment_free(&src);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_fragment_filter_keeps_nothing(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t src = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t dst = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_fragment_parse(client, &src, &vars, "-I/usr/include -lfoo", 0);

	pkgconf_fragment_filter(client, &dst, &src, filter_nothing, NULL);

	TEST_ASSERT_EQ(fragment_count(&dst), 0);

	pkgconf_fragment_free(&dst);
	pkgconf_fragment_free(&src);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_fragment_has_system_dir_matches(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t frags = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_path_add("/usr/include", &client->filter_includedirs, false);

	pkgconf_fragment_parse(client, &frags, &vars, "-I/usr/include -I/opt/include", 0);

	const pkgconf_fragment_t *system = fragment_at(&frags, 0);
	const pkgconf_fragment_t *other = fragment_at(&frags, 1);
	TEST_ASSERT_NONNULL(system);
	TEST_ASSERT_NONNULL(other);

	TEST_ASSERT_TRUE(pkgconf_fragment_has_system_dir(client, system));
	TEST_ASSERT_FALSE(pkgconf_fragment_has_system_dir(client, other));

	pkgconf_fragment_free(&frags);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

static void
test_fragment_has_system_dir_libs(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t frags = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t vars = PKGCONF_LIST_INITIALIZER;

	pkgconf_path_add("/usr/lib", &client->filter_libdirs, false);

	pkgconf_fragment_parse(client, &frags, &vars, "-L/usr/lib -L/opt/lib", 0);

	const pkgconf_fragment_t *system = fragment_at(&frags, 0);
	const pkgconf_fragment_t *other = fragment_at(&frags, 1);
	TEST_ASSERT_NONNULL(system);
	TEST_ASSERT_NONNULL(other);

	TEST_ASSERT_TRUE(pkgconf_fragment_has_system_dir(client, system));
	TEST_ASSERT_FALSE(pkgconf_fragment_has_system_dir(client, other));

	pkgconf_fragment_free(&frags);
	pkgconf_variable_list_free(&vars);
	pkgconf_client_free(client);
}

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_fragment_parse_empty);
	TEST_RUN(basename, test_fragment_parse_cflags);
	TEST_RUN(basename, test_fragment_parse_libs);
	TEST_RUN(basename, test_fragment_add_single);
	TEST_RUN(basename, test_fragment_render_empty);
	TEST_RUN(basename, test_fragment_render_cflags);
	TEST_RUN(basename, test_fragment_render_libs);
	TEST_RUN(basename, test_fragment_filter_only_includes);
	TEST_RUN(basename, test_fragment_filter_only_libnames);
	TEST_RUN(basename, test_fragment_filter_keeps_nothing);
	TEST_RUN(basename, test_fragment_has_system_dir_matches);
	TEST_RUN(basename, test_fragment_has_system_dir_libs);

	return EXIT_SUCCESS; 
}
