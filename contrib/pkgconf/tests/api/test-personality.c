/*
 * test-personality.c
 * Tests for libpkgconf personality functions.
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
test_personality_deinit_null(void)
{
	// Should not crash
	pkgconf_cross_personality_deinit(NULL);
}

static void
test_personality_default_refcount(void)
{
	pkgconf_cross_personality_t *p1 = pkgconf_cross_personality_default();
	pkgconf_cross_personality_t *p2 = pkgconf_cross_personality_default();

	TEST_ASSERT_NONNULL(p1);
	TEST_ASSERT_NONNULL(p2);
	// Both calls must return the same singleton pointer
	TEST_ASSERT_EQ((long long)(uintptr_t)p1, (long long)(uintptr_t)p2);

	// Refcount is 2 -> first deinit drops it to 1, must not free
	pkgconf_cross_personality_deinit(p1);
	// Refcount is 1 -> second deinit drops to 0, frees internal state
	pkgconf_cross_personality_deinit(p2);

	// Re-initialise from scratch
	pkgconf_cross_personality_t *p3 = pkgconf_cross_personality_default();
	TEST_ASSERT_NONNULL(p3);
	pkgconf_cross_personality_deinit(p3);
}

#ifndef PKGCONF_LITE

static void
test_personality_find_invalid_triplet(void)
{
	/* "bad triplet!" has a space and '!' — both rejected by valid_triplet,
	 * and the direct-file open will also fail, so result must be NULL. */
	pkgconf_cross_personality_t *p = pkgconf_cross_personality_find("bad triplet!");
	TEST_ASSERT_NULL(p);
}

static void
test_personality_find_direct_path(void)
{
	char path[] = "test-personality-XXXXXX";
	int fd = mkstemp(path);
	TEST_ASSERT_TRUE(fd >= 0);

	FILE *f = fdopen(fd, "w");
	TEST_ASSERT_NONNULL(f);

	fprintf(f,
		"Triplet: x86_64-linux-musl\n"
		"DefaultSearchPaths: /usr/lib/pkgconfig:/usr/share/pkgconfig\n"
		"SysrootDir: /opt/sysroot\n"
		"SystemIncludePaths: /opt/sysroot/usr/include\n"
		"SystemLibraryPaths: /opt/sysroot/usr/lib\n"
		"WantDefaultStatic: true\n"
		"WantDefaultPure: yes\n");
	fclose(f);

	pkgconf_cross_personality_t *p = pkgconf_cross_personality_find(path);
	TEST_ASSERT_NONNULL(p);
	TEST_ASSERT_STRCMP_EQ(p->name, "x86_64-linux-musl");
	TEST_ASSERT_NONNULL(p->sysroot_dir);
	TEST_ASSERT_STRCMP_EQ(p->sysroot_dir, "/opt/sysroot");
	TEST_ASSERT_TRUE(p->want_default_static);
	TEST_ASSERT_TRUE(p->want_default_pure);
	TEST_ASSERT_NONNULL(p->dir_list.head);

	// Exercises the non-default free path
	pkgconf_cross_personality_deinit(p);
	unlink(path);
}

#if !defined(_WIN32) && !defined(__HAIKU__)

static void
test_personality_find_via_xdg(void)
{
	char tmpdir[] = "test-personality-xdg-XXXXXX";
	char *d = mkdtemp(tmpdir);
	TEST_ASSERT_NONNULL(d);

	// build: <tmpdir>/pkgconfig/personality.d/
	char pkgconfig_dir[4096];
	char personality_d[4096];
	char file_path[4096];

	snprintf(pkgconfig_dir, sizeof pkgconfig_dir, "%s/pkgconfig", tmpdir);
	snprintf(personality_d, sizeof personality_d, "%s/pkgconfig/personality.d", tmpdir);
	snprintf(file_path, sizeof file_path, "%s/pkgconfig/personality.d/xdg-test-triplet.personality", tmpdir);

	TEST_ASSERT_EQ(mkdir(pkgconfig_dir, 0755), 0);
	TEST_ASSERT_EQ(mkdir(personality_d, 0755), 0);

	FILE *f = fopen(file_path, "w");
	TEST_ASSERT_NONNULL(f);
	fprintf(f, "SysrootDir: /xdg/sysroot\n");
	fclose(f);

	setenv("XDG_DATA_HOME", tmpdir, 1);

	pkgconf_cross_personality_t *p = pkgconf_cross_personality_find("xdg-test-triplet");
	TEST_ASSERT_NONNULL(p);
	TEST_ASSERT_NONNULL(p->sysroot_dir);
	TEST_ASSERT_STRCMP_EQ(p->sysroot_dir, "/xdg/sysroot");

	// Verify it is NOT the default singleton
	pkgconf_cross_personality_t *def = pkgconf_cross_personality_default();
	TEST_ASSERT_NE((long long)(uintptr_t)p, (long long)(uintptr_t)def);
	// Balance the _default() call above
	pkgconf_cross_personality_deinit(def);

	// Clean up
	pkgconf_cross_personality_deinit(p);
	unsetenv("XDG_DATA_HOME");

	unlink(file_path);
	rmdir(personality_d);
	rmdir(pkgconfig_dir);
	rmdir(tmpdir);
}

#endif // !_WIN32 && !__HAIKU__

static void
test_personality_find_missing_returns_default(void)
{
	// clear XDG / HOME so search goes to known-empty places
	unsetenv("XDG_DATA_HOME");
	unsetenv("XDG_DATA_DIRS");
	unsetenv("HOME");

	pkgconf_cross_personality_t *found = pkgconf_cross_personality_find("nonexistent-triplet-xyzzy");
	TEST_ASSERT_NONNULL(found);

	// The find internally called _default(); call it ourselves to compare
	pkgconf_cross_personality_t *def = pkgconf_cross_personality_default();
	TEST_ASSERT_EQ((long long)(uintptr_t)found, (long long)(uintptr_t)def);

	// Two deinits: one for find's internal _default(), one for ours above
	pkgconf_cross_personality_deinit(found);
	pkgconf_cross_personality_deinit(def);
}

#endif /* !PKGCONF_LITE */

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	// refcount test must run first so the singleton starts at 0
	TEST_RUN(basename, test_personality_deinit_null);
	TEST_RUN(basename, test_personality_default_refcount);

#ifndef PKGCONF_LITE
	TEST_RUN(basename, test_personality_find_invalid_triplet);
	TEST_RUN(basename, test_personality_find_direct_path);
#if !defined(_WIN32) && !defined(__HAIKU__)
	TEST_RUN(basename, test_personality_find_via_xdg);
#endif
	TEST_RUN(basename, test_personality_find_missing_returns_default);
#endif

	return EXIT_SUCCESS;
}
