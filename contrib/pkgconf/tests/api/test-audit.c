/*
 * test-audit.c
 * Tests for the public libpkgconf audit API.
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

/*
 * Read the entire contents of f (from the start) into buf.
 * buf must be large enough; we keep test inputs small.
 */
static void
slurp(FILE *f, char *buf, size_t bufsz)
{
	rewind(f);
	size_t n = fread(buf, 1, bufsz - 1, f);
	buf[n] = '\0';
}

static void
test_audit_log_no_logfile_is_noop(void)
{
	pkgconf_client_t *client = test_client_new();

	// With no audit log set, logging must be a silent no-op and must not crash.
	pkgconf_audit_log(client, "should go nowhere\n");

	pkgconf_client_free(client);
}

static void
test_audit_set_log_and_write(void)
{
	pkgconf_client_t *client = test_client_new();
	FILE *logf = tmpfile();
	TEST_ASSERT_NONNULL(logf);

	pkgconf_audit_set_log(client, logf);
	pkgconf_audit_log(client, "hello %s\n", "world");

	char buf[256];
	slurp(logf, buf, sizeof(buf));
	TEST_ASSERT_STRCMP_EQ(buf, "hello world\n");

	fclose(logf);
	pkgconf_client_free(client);
}

static void
test_audit_log_multiple_writes(void)
{
	pkgconf_client_t *client = test_client_new();
	FILE *logf = tmpfile();
	TEST_ASSERT_NONNULL(logf);

	pkgconf_audit_set_log(client, logf);
	pkgconf_audit_log(client, "first\n");
	pkgconf_audit_log(client, "second %d\n", 2);

	char buf[256];
	slurp(logf, buf, sizeof(buf));
	TEST_ASSERT_STRCMP_EQ(buf, "first\nsecond 2\n");

	fclose(logf);
	pkgconf_client_free(client);
}

static void
test_audit_log_dependency_versionless(void)
{
	pkgconf_client_t *client = test_client_new();
	FILE *logf = tmpfile();
	TEST_ASSERT_NONNULL(logf);

	pkgconf_audit_set_log(client, logf);

	// A minimal package: only the fields the logger reads
	pkgconf_pkg_t pkg = { 0 };
	pkg.id = (char *) "foo";
	pkg.version = (char *) "1.2.3";

	/* A dependency with PKGCONF_CMP_ANY and no version: the logger
	 * should emit only "id [version]", skipping the comparator */
	pkgconf_dependency_t dep = { 0 };
	dep.compare = PKGCONF_CMP_ANY;
	dep.version = NULL;

	pkgconf_audit_log_dependency(client, &pkg, &dep);

	char buf[256];
	slurp(logf, buf, sizeof(buf));
	TEST_ASSERT_STRCMP_EQ(buf, "foo [1.2.3]\n");

	fclose(logf);
	pkgconf_client_free(client);
}

static void
test_audit_log_dependency_versioned(void)
{
	pkgconf_client_t *client = test_client_new();
	FILE *logf = tmpfile();
	TEST_ASSERT_NONNULL(logf);

	pkgconf_audit_set_log(client, logf);

	pkgconf_pkg_t pkg = { 0 };
	pkg.id = (char *) "bar";
	pkg.version = (char *) "2.0";

	/* version set AND compare != ANY: the logger emits the
	 * comparator and required version between id and [version] */
	pkgconf_dependency_t dep = { 0 };
	dep.compare = PKGCONF_CMP_GREATER_THAN_EQUAL;
	dep.version = (char *) "1.5";

	pkgconf_audit_log_dependency(client, &pkg, &dep);

	char buf[256];
	slurp(logf, buf, sizeof(buf));
	TEST_ASSERT_STRCMP_EQ(buf, "bar >= 1.5 [2.0]\n");

	fclose(logf);
	pkgconf_client_free(client);
}

static void
test_audit_log_dependency_no_logfile_is_noop(void)
{
	pkgconf_client_t *client = test_client_new();

	pkgconf_pkg_t pkg = { 0 };
	pkg.id = (char *) "foo";
	pkg.version = (char *) "1.0";

	pkgconf_dependency_t dep = { 0 };
	dep.compare = PKGCONF_CMP_ANY;

	// No log set: must be a silent no-op
	pkgconf_audit_log_dependency(client, &pkg, &dep);

	pkgconf_client_free(client);
}

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_audit_log_no_logfile_is_noop);
	TEST_RUN(basename, test_audit_set_log_and_write);
	TEST_RUN(basename, test_audit_log_multiple_writes);
	TEST_RUN(basename, test_audit_log_dependency_versionless);
	TEST_RUN(basename, test_audit_log_dependency_versioned);
	TEST_RUN(basename, test_audit_log_dependency_no_logfile_is_noop);

	return EXIT_SUCCESS;
}
