/*
 * test-client.c
 * Tests for the public libpkgconf client API.
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
test_client_new_and_free(void)
{
	pkgconf_cross_personality_t *pers = pkgconf_cross_personality_default();
	pkgconf_client_t *client = pkgconf_client_new(NULL, NULL, pers, NULL, NULL);
	TEST_ASSERT_NONNULL(client);

	pkgconf_client_free(client);
}

static void
test_client_init_and_deinit_stack(void)
{
	/* Caller-allocated client, the path the CLI itself uses
	 * (pkgconf_cli_state_t embeds a pkgconf_client_t by value) */
	pkgconf_client_t client = { 0 };
	pkgconf_cross_personality_t *pers = pkgconf_cross_personality_default();

	pkgconf_client_init(&client, NULL, NULL, pers, NULL, NULL);
	pkgconf_client_deinit(&client);
}

static void
test_client_sysroot_dir(void)
{
	pkgconf_client_t *client = test_client_new();

	pkgconf_client_set_sysroot_dir(client, "/tmp/sysroot");
	TEST_ASSERT_STRCMP_EQ(pkgconf_client_get_sysroot_dir(client), "/tmp/sysroot");

	pkgconf_client_set_sysroot_dir(client, "/opt/sysroot");
	TEST_ASSERT_STRCMP_EQ(pkgconf_client_get_sysroot_dir(client), "/opt/sysroot");

	pkgconf_client_free(client);
}

static void
test_client_buildroot_dir(void)
{
	pkgconf_client_t *client = test_client_new();

	pkgconf_client_set_buildroot_dir(client, "/tmp/buildroot");
	TEST_ASSERT_STRCMP_EQ(pkgconf_client_get_buildroot_dir(client), "/tmp/buildroot");

	pkgconf_client_free(client);
}

static void
test_client_flags(void)
{
	pkgconf_client_t *client = test_client_new();

	pkgconf_client_set_flags(client, PKGCONF_PKG_PKGF_NO_CACHE | PKGCONF_PKG_PKGF_SKIP_CONFLICTS);

	unsigned int flags = pkgconf_client_get_flags(client);
	TEST_ASSERT_NE(flags & PKGCONF_PKG_PKGF_NO_CACHE, 0);
	TEST_ASSERT_NE(flags & PKGCONF_PKG_PKGF_SKIP_CONFLICTS, 0);
	TEST_ASSERT_EQ(flags & PKGCONF_PKG_PKGF_ENV_ONLY, 0);

	pkgconf_client_free(client);
}

static void
test_client_prefix_varname(void)
{
	pkgconf_client_t *client = test_client_new();

	pkgconf_client_set_prefix_varname(client, "prefix");
	TEST_ASSERT_STRCMP_EQ(pkgconf_client_get_prefix_varname(client), "prefix");

	pkgconf_client_set_prefix_varname(client, "custom_prefix");
	TEST_ASSERT_STRCMP_EQ(pkgconf_client_get_prefix_varname(client), "custom_prefix");

	pkgconf_client_free(client);
}

/*
 * Capture buffer for handler tests.  Each handler writes the message
 * into this buffer so the test can verify it fired with the expected
 * content.  Reset before each test that uses it.
 */
static char capture_buf[256];

static bool
capture_handler(const char *msg, const pkgconf_client_t *client, void *data)
{
	(void) client;
	(void) data;

	strncpy(capture_buf, msg, sizeof(capture_buf) - 1);
	capture_buf[sizeof(capture_buf) - 1] = '\0';
	return true;
}

static void
capture_reset(void)
{
	memset(capture_buf, 0, sizeof(capture_buf));
}

static void
test_client_error_handler_fires(void)
{
	pkgconf_client_t *client = test_client_new();
	capture_reset();

	pkgconf_client_set_error_handler(client, capture_handler, NULL);

	TEST_ASSERT_EQ(pkgconf_client_get_error_handler(client), capture_handler);

	pkgconf_error(client, "test error: %d", 42);

	TEST_ASSERT_NE(capture_buf[0], '\0');
	TEST_ASSERT_STRSTR(capture_buf, "test error: 42");

	pkgconf_client_free(client);
}

static void
test_client_warn_handler_fires(void)
{
	pkgconf_client_t *client = test_client_new();
	capture_reset();

	pkgconf_client_set_warn_handler(client, capture_handler, NULL);

	TEST_ASSERT_EQ(pkgconf_client_get_warn_handler(client), capture_handler);

	pkgconf_warn(client, "test warning: %s", "hello");

	TEST_ASSERT_NE(capture_buf[0], '\0');
	TEST_ASSERT_STRSTR(capture_buf, "test warning: hello");

	pkgconf_client_free(client);
}

#ifndef PKGCONF_LITE
static void
test_client_trace_handler_fires(void)
{
	pkgconf_client_t *client = test_client_new();
	capture_reset();

	pkgconf_client_set_trace_handler(client, capture_handler, NULL);

	TEST_ASSERT_EQ(pkgconf_client_get_trace_handler(client), capture_handler);

	PKGCONF_TRACE(client, "trace message: %d", 7);

	TEST_ASSERT_NE(capture_buf[0], '\0');
	TEST_ASSERT_STRSTR(capture_buf, "trace message: 7");

	pkgconf_client_free(client);
}
#endif

static void
unveil_capture_handler(const pkgconf_client_t *client, const char *path, const char *permissions)
{
	(void) client;
	(void) permissions;

	if (path != NULL)
	{
		strncpy(capture_buf, path, sizeof(capture_buf) - 1);
		capture_buf[sizeof(capture_buf) - 1] = '\0';
	}
}

static void
test_client_unveil_handler_installation(void)
{
	pkgconf_client_t *client = test_client_new();

	pkgconf_client_set_unveil_handler(client, unveil_capture_handler);
	TEST_ASSERT_EQ(pkgconf_client_get_unveil_handler(client), unveil_capture_handler);

	pkgconf_client_free(client);
}

/*
 * Custom environ handler that returns canned values for known keys
 * and NULL otherwise. 
 */
static const char *
canned_environ_handler(const pkgconf_client_t *client, const char *key)
{
	(void) client;

	if (!strcmp(key, "PKG_TEST_VAR"))
		return "the_value";
	if (!strcmp(key, "EMPTY_VAR"))
		return "";
	if (!strcmp(key, "PKG_CONFIG_SYSTEM_INCLUDE_PATH"))
		return "/custom/include";
	if (!strcmp(key, "PKG_CONFIG_SYSTEM_LIBRARY_PATH"))
		return "/custom/lib";

	return NULL;
}

static void
test_client_getenv_via_handler(void)
{
	pkgconf_cross_personality_t *pers = pkgconf_cross_personality_default();
	pkgconf_client_t *client = pkgconf_client_new(NULL, NULL, pers, NULL, canned_environ_handler);
	TEST_ASSERT_NONNULL(client);

	const char *v = pkgconf_client_getenv(client, "PKG_TEST_VAR");
	TEST_ASSERT_NONNULL(v);
	TEST_ASSERT_STRCMP_EQ(v, "the_value");

	v = pkgconf_client_getenv(client, "EMPTY_VAR");
	TEST_ASSERT_NONNULL(v);
	TEST_ASSERT_STRCMP_EQ(v, "");

	v = pkgconf_client_getenv(client, "UNDEFINED_VAR");
	TEST_ASSERT_NULL(v);

	pkgconf_client_free(client);
}

static void
test_client_dir_list_build_smoke(void)
{
	pkgconf_cross_personality_t *pers = pkgconf_cross_personality_default();
	pkgconf_client_t *client = pkgconf_client_new(NULL, NULL, pers, NULL, NULL);
	TEST_ASSERT_NONNULL(client);

	pkgconf_client_dir_list_build(client, pers);

	/* The personality's default dir list comes from PKG_DEFAULT_PATH
	 * (set at compile time).  After build, the client should have
	 * SOME directories registered; we don't assert specific paths
	 * since they vary by build configuration. */
	TEST_ASSERT_NONNULL(client->dir_list.head);

	pkgconf_client_free(client);
}

static void
test_client_init_system_paths_from_environ(void)
{
	pkgconf_cross_personality_t *pers = pkgconf_cross_personality_default();
	pkgconf_client_t *client = pkgconf_client_new(NULL, NULL, pers, NULL, canned_environ_handler);
	TEST_ASSERT_NONNULL(client);

	/* With PKG_CONFIG_SYSTEM_{INCLUDE,LIBRARY}_PATH set, init builds the filter dirs from the 
	 * environment instead of copying the personality's defaults. */
	TEST_ASSERT_TRUE(pkgconf_path_match_list("/custom/include", &client->filter_includedirs));
	TEST_ASSERT_TRUE(pkgconf_path_match_list("/custom/lib", &client->filter_libdirs));

	pkgconf_client_free(client);
}

static void
test_client_preload_from_environ(void)
{
	pkgconf_cross_personality_t *pers = pkgconf_cross_personality_default();
	pkgconf_client_t *client = pkgconf_client_new(NULL, NULL, pers, NULL, NULL);
	TEST_ASSERT_NONNULL(client);

	/* preload_from_environ reads the named var via getenv directly.
	 * Point it at a dir; preload_path will try to load .pc files from there.
	 * An empty/nonexistent dir is fine; we're exercising the split-and-iterate path, not asserting
	 * loads. */
	setenv("PKG_TEST_PRELOAD", "/nonexistent/dir", 1);

	pkgconf_client_preload_from_environ(client, "PKG_TEST_PRELOAD");

	unsetenv("PKG_TEST_PRELOAD");
	pkgconf_client_free(client);
}

#ifndef PKGCONF_LITE
static void
test_client_trace_null_client(void)
{
	TEST_ASSERT_FALSE(pkgconf_trace(NULL, "test.c", 42, "func", "msg %d", 42));
}
#endif

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_client_new_and_free);
	TEST_RUN(basename, test_client_init_and_deinit_stack);
	TEST_RUN(basename, test_client_init_system_paths_from_environ);
	TEST_RUN(basename, test_client_preload_from_environ);

	TEST_RUN(basename, test_client_sysroot_dir);
	TEST_RUN(basename, test_client_buildroot_dir);
	TEST_RUN(basename, test_client_flags);
	TEST_RUN(basename, test_client_prefix_varname);

	TEST_RUN(basename, test_client_error_handler_fires);
	TEST_RUN(basename, test_client_warn_handler_fires);
#ifndef PKGCONF_LITE
	TEST_RUN(basename, test_client_trace_handler_fires);
#endif
	TEST_RUN(basename, test_client_unveil_handler_installation);

	TEST_RUN(basename, test_client_getenv_via_handler);

	TEST_RUN(basename, test_client_dir_list_build_smoke);

#ifndef PKGCONF_LITE
	TEST_RUN(basename, test_client_trace_null_client);
#endif

	return EXIT_SUCCESS;
}
