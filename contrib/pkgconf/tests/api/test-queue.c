/*
 * test-queue.c
 * Tests for the public libpkgconf queue API.
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

#define FIXTURE_DIR "test-queue-pcdir"

static void
write_pc(const char *name, const char *contents)
{
	char path[512];
	FILE *f;

	snprintf(path, sizeof path, "%s/%s", FIXTURE_DIR, name);
	f = fopen(path, "wb");
	TEST_ASSERT_NONNULL(f);
	fwrite(contents, 1, strlen(contents), f);
	fclose(f);
}

static void
setup_fixtures(void)
{
	mkdir(FIXTURE_DIR, 0755);
	write_pc("qbar.pc", "Name: qbar\nDescription: bar\nVersion: 2.0\n");
	write_pc("qfoo.pc",
		"Name: qfoo\nDescription: foo\nVersion: 1.0\nRequires: qbar >= 1.0\n");
}

static void
teardown_fixtures(void)
{
	remove(FIXTURE_DIR "/qfoo.pc");
	remove(FIXTURE_DIR "/qbar.pc");
	rmdir(FIXTURE_DIR);
}

static pkgconf_client_t *
fixture_client(void)
{
	pkgconf_client_t *client = test_client_new();

	pkgconf_path_free(&client->dir_list);
	pkgconf_path_add(FIXTURE_DIR, &client->dir_list, false);

	return client;
}

static size_t
required_count(const pkgconf_pkg_t *world)
{
	size_t n = 0;
	const pkgconf_node_t *iter;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		n++;
	}

	return n;
}

static bool
contains_dep(const pkgconf_pkg_t *world, const char *name)
{
	const pkgconf_node_t *iter;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, iter)
	{
		const pkgconf_dependency_t *dep = iter->data;

		if (!strcmp(dep->package, name))
			return true;
	}

	return false;
}

static void
test_queue_validate_success(void)
{
	pkgconf_client_t *client = fixture_client();
	pkgconf_list_t queue = PKGCONF_LIST_INITIALIZER;

	pkgconf_queue_push(&queue, "qfoo");
	TEST_ASSERT_TRUE(pkgconf_queue_validate(client, &queue, -1));

	pkgconf_queue_free(&queue);
	pkgconf_client_free(client);
}

static void
test_queue_validate_version_satisfied(void)
{
	pkgconf_client_t *client = fixture_client();
	pkgconf_list_t queue = PKGCONF_LIST_INITIALIZER;

	pkgconf_queue_push(&queue, "qbar >= 1.0");
	TEST_ASSERT_TRUE(pkgconf_queue_validate(client, &queue, -1));

	pkgconf_queue_free(&queue);
	pkgconf_client_free(client);
}

static void
test_queue_validate_missing_package(void)
{
	pkgconf_client_t *client = fixture_client();
	pkgconf_list_t queue = PKGCONF_LIST_INITIALIZER;

	pkgconf_queue_push(&queue, "does-not-exist");
	TEST_ASSERT_FALSE(pkgconf_queue_validate(client, &queue, -1));

	pkgconf_queue_free(&queue);
	pkgconf_client_free(client);
}

static void
test_queue_validate_unsatisfiable_version(void)
{
	pkgconf_client_t *client = fixture_client();
	pkgconf_list_t queue = PKGCONF_LIST_INITIALIZER;

	pkgconf_queue_push(&queue, "qbar >= 99.0");
	TEST_ASSERT_FALSE(pkgconf_queue_validate(client, &queue, -1));

	pkgconf_queue_free(&queue);
	pkgconf_client_free(client);
}

static void
test_queue_validate_empty(void)
{
	pkgconf_client_t *client = fixture_client();
	pkgconf_list_t queue = PKGCONF_LIST_INITIALIZER;

	TEST_ASSERT_FALSE(pkgconf_queue_validate(client, &queue, -1));

	pkgconf_queue_free(&queue);
	pkgconf_client_free(client);
}

struct apply_state {
	int calls;
	size_t deps;
	bool saw_qfoo;
	bool saw_qbar;
	bool retval;
};

static bool
apply_cb(pkgconf_client_t *client, pkgconf_pkg_t *world, void *data, int maxdepth)
{
	struct apply_state *st = data;

	(void) client;
	(void) maxdepth;

	st->calls++;
	st->deps = required_count(world);
	st->saw_qfoo = contains_dep(world, "qfoo");
	st->saw_qbar = contains_dep(world, "qbar");

	return st->retval;
}

static void
test_queue_apply_success(void)
{
	pkgconf_client_t *client = fixture_client();
	pkgconf_list_t queue = PKGCONF_LIST_INITIALIZER;
	struct apply_state st = { .retval = true };

	pkgconf_queue_push(&queue, "qfoo");
	TEST_ASSERT_TRUE(pkgconf_queue_apply(client, &queue, apply_cb, -1, &st));

	TEST_ASSERT_EQ(st.calls, 1);
	TEST_ASSERT_GE(st.deps, 2);
	TEST_ASSERT_TRUE(st.saw_qfoo);
	TEST_ASSERT_TRUE(st.saw_qbar);

	pkgconf_queue_free(&queue);
	pkgconf_client_free(client);
}

static void
test_queue_apply_callback_failure(void)
{
	pkgconf_client_t *client = fixture_client();
	pkgconf_list_t queue = PKGCONF_LIST_INITIALIZER;
	struct apply_state st = { .retval = false };

	pkgconf_queue_push(&queue, "qfoo");
	TEST_ASSERT_FALSE(pkgconf_queue_apply(client, &queue, apply_cb, -1, &st));
	TEST_ASSERT_EQ(st.calls, 1);

	pkgconf_queue_free(&queue);
	pkgconf_client_free(client);
}

static void
test_queue_apply_missing_package(void)
{
	pkgconf_client_t *client = fixture_client();
	pkgconf_list_t queue = PKGCONF_LIST_INITIALIZER;
	struct apply_state st = { .retval = true };

	pkgconf_queue_push(&queue, "does-not-exist");
	TEST_ASSERT_FALSE(pkgconf_queue_apply(client, &queue, apply_cb, -1, &st));
	TEST_ASSERT_EQ(st.calls, 0);

	pkgconf_queue_free(&queue);
	pkgconf_client_free(client);
}

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	setup_fixtures();

	TEST_RUN(basename, test_queue_validate_success);
	TEST_RUN(basename, test_queue_validate_version_satisfied);
	TEST_RUN(basename, test_queue_validate_missing_package);
	TEST_RUN(basename, test_queue_validate_unsatisfiable_version);
	TEST_RUN(basename, test_queue_validate_empty);
	TEST_RUN(basename, test_queue_apply_success);
	TEST_RUN(basename, test_queue_apply_callback_failure);
	TEST_RUN(basename, test_queue_apply_missing_package);

	teardown_fixtures();

	return EXIT_SUCCESS;
}
