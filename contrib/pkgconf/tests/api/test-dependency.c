/*
 * test-dependency.c
 * Tests for the public libpkgconf dependency API.
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
dependency_count(const pkgconf_list_t *list)
{
	size_t n = 0;
	const pkgconf_node_t *iter;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, iter)
	{
		n++;
	}

	return n;
}

static const pkgconf_dependency_t *
dependency_at(const pkgconf_list_t *list, size_t index)
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

static void
test_comparator_lookup_known(void)
{
	TEST_ASSERT_EQ(pkgconf_pkg_comparator_lookup_by_name("="), PKGCONF_CMP_EQUAL);
	TEST_ASSERT_EQ(pkgconf_pkg_comparator_lookup_by_name("!="), PKGCONF_CMP_NOT_EQUAL);
	TEST_ASSERT_EQ(pkgconf_pkg_comparator_lookup_by_name("<"), PKGCONF_CMP_LESS_THAN);
	TEST_ASSERT_EQ(pkgconf_pkg_comparator_lookup_by_name("<="), PKGCONF_CMP_LESS_THAN_EQUAL);
	TEST_ASSERT_EQ(pkgconf_pkg_comparator_lookup_by_name(">"), PKGCONF_CMP_GREATER_THAN);
	TEST_ASSERT_EQ(pkgconf_pkg_comparator_lookup_by_name(">="), PKGCONF_CMP_GREATER_THAN_EQUAL);
}

static void
test_comparator_lookup_unknown(void)
{
	TEST_ASSERT_EQ(pkgconf_pkg_comparator_lookup_by_name("~~"), PKGCONF_CMP_ANY);
	TEST_ASSERT_EQ(pkgconf_pkg_comparator_lookup_by_name(""), PKGCONF_CMP_ANY);
}

static void
test_comparator_roundtrip(void)
{
	pkgconf_pkg_comparator_t values[] =
	{
		PKGCONF_CMP_NOT_EQUAL,
		PKGCONF_CMP_ANY,
		PKGCONF_CMP_LESS_THAN,
		PKGCONF_CMP_LESS_THAN_EQUAL,
		PKGCONF_CMP_EQUAL,
		PKGCONF_CMP_GREATER_THAN,
		PKGCONF_CMP_GREATER_THAN_EQUAL,
	};

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(values); i++)
	{
		pkgconf_dependency_t dep = { 0 };
		dep.compare = values[i];

		const char *str = pkgconf_pkg_get_comparator(&dep);
		TEST_ASSERT_NONNULL(str);

		// ANY's value does not round-trip so do not check
		if (values[i] == PKGCONF_CMP_ANY)
			continue;

		pkgconf_pkg_comparator_t back = pkgconf_pkg_comparator_lookup_by_name(str);
		TEST_ASSERT_EQ(back, values[i]);
	}
}

static void
test_parse_str_single(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_parse_str(client, &deps, "foo", 0);

	TEST_ASSERT_EQ(dependency_count(&deps), 1);
	const pkgconf_dependency_t *d = dependency_at(&deps, 0);
	TEST_ASSERT_NONNULL(d);
	TEST_ASSERT_STRCMP_EQ(d->package, "foo");
	TEST_ASSERT_EQ(d->compare, PKGCONF_CMP_ANY);
	TEST_ASSERT_NULL(d->version);

	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_parse_str_versioned(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_parse_str(client, &deps, "foo >= 1.2.3", 0);

	TEST_ASSERT_EQ(dependency_count(&deps), 1);
	const pkgconf_dependency_t *d = dependency_at(&deps, 0);
	TEST_ASSERT_NONNULL(d);
	TEST_ASSERT_STRCMP_EQ(d->package, "foo");
	TEST_ASSERT_EQ(d->compare, PKGCONF_CMP_GREATER_THAN_EQUAL);
	TEST_ASSERT_STRCMP_EQ(d->version, "1.2.3");

	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_parse_str_multiple_space_separated(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_parse_str(client, &deps, "foo bar baz", 0);

	TEST_ASSERT_EQ(dependency_count(&deps), 3);

	const pkgconf_dependency_t *d0 = dependency_at(&deps, 0);
	const pkgconf_dependency_t *d1 = dependency_at(&deps, 1);
	const pkgconf_dependency_t *d2 = dependency_at(&deps, 2);
	TEST_ASSERT_NONNULL(d0);
	TEST_ASSERT_NONNULL(d1);
	TEST_ASSERT_NONNULL(d2);
	TEST_ASSERT_STRCMP_EQ(d0->package, "foo");
	TEST_ASSERT_STRCMP_EQ(d1->package, "bar");
	TEST_ASSERT_STRCMP_EQ(d2->package, "baz");

	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_parse_str_multiple_comma_separated(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_parse_str(client, &deps, "foo, bar, baz", 0);

	TEST_ASSERT_EQ(dependency_count(&deps), 3);

	const pkgconf_dependency_t *d0 = dependency_at(&deps, 0);
	const pkgconf_dependency_t *d1 = dependency_at(&deps, 1);
	const pkgconf_dependency_t *d2 = dependency_at(&deps, 2);
	TEST_ASSERT_NONNULL(d0);
	TEST_ASSERT_NONNULL(d1);
	TEST_ASSERT_NONNULL(d2);
	TEST_ASSERT_STRCMP_EQ(d0->package, "foo");
	TEST_ASSERT_STRCMP_EQ(d1->package, "bar");
	TEST_ASSERT_STRCMP_EQ(d2->package, "baz");

	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_parse_str_mixed_versioned_and_bare(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_parse_str(client, &deps, "foo >= 1.0, bar, baz < 2.0", 0);

	TEST_ASSERT_EQ(dependency_count(&deps), 3);

	const pkgconf_dependency_t *d0 = dependency_at(&deps, 0);
	const pkgconf_dependency_t *d1 = dependency_at(&deps, 1);
	const pkgconf_dependency_t *d2 = dependency_at(&deps, 2);
	
	TEST_ASSERT_NONNULL(d0);
	TEST_ASSERT_NONNULL(d1);
	TEST_ASSERT_NONNULL(d2);

	TEST_ASSERT_STRCMP_EQ(d0->package, "foo");
	TEST_ASSERT_EQ(d0->compare, PKGCONF_CMP_GREATER_THAN_EQUAL);
	TEST_ASSERT_STRCMP_EQ(d0->version, "1.0");

	TEST_ASSERT_STRCMP_EQ(d1->package, "bar");
	TEST_ASSERT_EQ(d1->compare, PKGCONF_CMP_ANY);

	TEST_ASSERT_STRCMP_EQ(d2->package, "baz");
	TEST_ASSERT_EQ(d2->compare, PKGCONF_CMP_LESS_THAN);
	TEST_ASSERT_STRCMP_EQ(d2->version, "2.0");

	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_parse_str_empty(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_parse_str(client, &deps, "", 0);
	TEST_ASSERT_EQ(dependency_count(&deps), 0);

	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_parse_str_all_comparators(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_parse_str(client, &deps, "a = 1, b != 2, c < 3, d <= 4, e > 5, f >= 6", 0);

	TEST_ASSERT_EQ(dependency_count(&deps), 6);

	const pkgconf_dependency_t *d0 = dependency_at(&deps, 0);
	const pkgconf_dependency_t *d1 = dependency_at(&deps, 1);
	const pkgconf_dependency_t *d2 = dependency_at(&deps, 2);
	const pkgconf_dependency_t *d3 = dependency_at(&deps, 3);
	const pkgconf_dependency_t *d4 = dependency_at(&deps, 4);
	const pkgconf_dependency_t *d5 = dependency_at(&deps, 5);
	
	TEST_ASSERT_NONNULL(d0);
	TEST_ASSERT_NONNULL(d1);
	TEST_ASSERT_NONNULL(d2);
	TEST_ASSERT_NONNULL(d3);
	TEST_ASSERT_NONNULL(d4);
	TEST_ASSERT_NONNULL(d5);

	TEST_ASSERT_EQ(d0->compare, PKGCONF_CMP_EQUAL);
	TEST_ASSERT_EQ(d1->compare, PKGCONF_CMP_NOT_EQUAL);
	TEST_ASSERT_EQ(d2->compare, PKGCONF_CMP_LESS_THAN);
	TEST_ASSERT_EQ(d3->compare, PKGCONF_CMP_LESS_THAN_EQUAL);
	TEST_ASSERT_EQ(d4->compare, PKGCONF_CMP_GREATER_THAN);
	TEST_ASSERT_EQ(d5->compare, PKGCONF_CMP_GREATER_THAN_EQUAL);

	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_dependency_add(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_t *d = pkgconf_dependency_add(client, &deps, "foo", "1.0", PKGCONF_CMP_GREATER_THAN_EQUAL, 0);
	TEST_ASSERT_NONNULL(d);
	TEST_ASSERT_STRCMP_EQ(d->package, "foo");
	TEST_ASSERT_STRCMP_EQ(d->version, "1.0");
	TEST_ASSERT_EQ(d->compare, PKGCONF_CMP_GREATER_THAN_EQUAL);

	TEST_ASSERT_EQ(dependency_count(&deps), 1);

	pkgconf_dependency_unref(client, d);
	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_dependency_add_no_version(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_t *d = pkgconf_dependency_add(client, &deps, "foo", NULL, PKGCONF_CMP_ANY, 0);
	TEST_ASSERT_NONNULL(d);
	TEST_ASSERT_STRCMP_EQ(d->package, "foo");
	TEST_ASSERT_EQ(d->compare, PKGCONF_CMP_ANY);

	pkgconf_dependency_unref(client, d);
	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_dependency_add_multiple(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_t *d0 = pkgconf_dependency_add(client, &deps, "foo", NULL, PKGCONF_CMP_ANY, 0);
	pkgconf_dependency_t *d1 = pkgconf_dependency_add(client, &deps, "bar", "2.0", PKGCONF_CMP_EQUAL, 0);
	pkgconf_dependency_t *d2 = pkgconf_dependency_add(client, &deps, "baz", "3.0", PKGCONF_CMP_LESS_THAN, 0);
	
	TEST_ASSERT_NONNULL(d0);
	TEST_ASSERT_NONNULL(d1);
	TEST_ASSERT_NONNULL(d2);

	TEST_ASSERT_EQ(dependency_count(&deps), 3);

	pkgconf_dependency_unref(client, d0);
	pkgconf_dependency_unref(client, d1);
	pkgconf_dependency_unref(client, d2);
	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_dependency_collision_drops_flagged_newcomer(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_t *first = pkgconf_dependency_add(client, &deps, "foo", "1.0", PKGCONF_CMP_EQUAL, 0);
	TEST_ASSERT_NONNULL(first);
	TEST_ASSERT_EQ(dependency_count(&deps), 1);

	/* Adding the same dep WITH flags collides; the flagged newcomer
	 * is dropped in favour of the existing unflagged node, so _add
	 * returns NULL and the count stays at 1 */
	pkgconf_dependency_t *second = pkgconf_dependency_add(client, &deps, "foo", "1.0", PKGCONF_CMP_EQUAL, PKGCONF_PKG_DEPF_INTERNAL);
	TEST_ASSERT_NULL(second);
	TEST_ASSERT_EQ(dependency_count(&deps), 1);

	pkgconf_dependency_unref(client, first);
	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_dependency_collision_drops_flagged_existing(void)
{
	pkgconf_client_t *client = test_client_new();
	pkgconf_list_t deps = PKGCONF_LIST_INITIALIZER;

	pkgconf_dependency_t *first = pkgconf_dependency_add(client, &deps, "foo", "1.0", PKGCONF_CMP_EQUAL, PKGCONF_PKG_DEPF_INTERNAL);
	TEST_ASSERT_NONNULL(first);
	TEST_ASSERT_EQ(dependency_count(&deps), 1);

	/* Adding the same dep UNFLAGGED collides; the existing flagged
	 * node is deleted and unref'd, the unflagged newcomer takes its
	 * place; count stays at 1, but the node is now the new one */
	pkgconf_dependency_t *second = pkgconf_dependency_add(client, &deps, "foo", "1.0", PKGCONF_CMP_EQUAL, 0);
	TEST_ASSERT_NONNULL(second);
	TEST_ASSERT_EQ(dependency_count(&deps), 1);
	TEST_ASSERT_EQ(second->flags, 0);

	pkgconf_dependency_unref(client, first);
	pkgconf_dependency_unref(client, second);
	pkgconf_dependency_free(&deps);
	pkgconf_client_free(client);
}

static void
test_version_equal(void)
{
	TEST_ASSERT_EQ(pkgconf_compare_version("1.0", "1.0"), 0);
	TEST_ASSERT_EQ(pkgconf_compare_version("1.2.3", "1.2.3"), 0);
	TEST_ASSERT_EQ(pkgconf_compare_version("", ""), 0);
}

static void
test_version_simple_numeric(void)
{
	TEST_ASSERT_LT(pkgconf_compare_version("1.0", "1.1"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("1.1", "1.0"), 0);

	TEST_ASSERT_LT(pkgconf_compare_version("1.0", "2.0"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("2.0", "1.0"), 0);

	TEST_ASSERT_LT(pkgconf_compare_version("1.2.3", "1.2.4"), 0);
	TEST_ASSERT_LT(pkgconf_compare_version("1.2.3", "1.3.0"), 0);
	TEST_ASSERT_LT(pkgconf_compare_version("1.2.3", "2.0.0"), 0);
}

static void
test_version_numeric_segments_not_lexical(void)
{
	TEST_ASSERT_GT(pkgconf_compare_version("1.10", "1.9"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("1.100", "1.99"), 0);
}

static void
test_version_different_lengths(void)
{
	TEST_ASSERT_LT(pkgconf_compare_version("1.0", "1.0.1"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("1.0.1", "1.0"), 0);
}

static void
test_version_alpha_suffix(void)
{
	TEST_ASSERT_GT(pkgconf_compare_version("1.0a", "1.0"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("1.0alpha", "1.0"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("1.0rc1", "1.0"), 0);

	TEST_ASSERT_LT(pkgconf_compare_version("1.0alpha", "1.0beta"), 0);
	TEST_ASSERT_LT(pkgconf_compare_version("1.0beta", "1.0rc"), 0);
}

static void
test_version_tilde_prerelease(void)
{
	TEST_ASSERT_LT(pkgconf_compare_version("1.0~rc1", "1.0"), 0);
	TEST_ASSERT_LT(pkgconf_compare_version("1.0~alpha", "1.0"), 0);
	TEST_ASSERT_LT(pkgconf_compare_version("1.0~beta", "1.0~rc"), 0);

	TEST_ASSERT_LT(pkgconf_compare_version("1.0~rc1", "1.0rc1"), 0);
}

static void
test_version_numeric_beats_alpha(void)
{
	TEST_ASSERT_GT(pkgconf_compare_version("1.0.1", "1.0a"), 0);
	TEST_ASSERT_LT(pkgconf_compare_version("1.0a", "1.0.1"), 0);
}

static void
test_version_alpha_ordering(void)
{
	TEST_ASSERT_LT(pkgconf_compare_version("1.0a", "1.0b"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("1.0b", "1.0a"), 0);
}

static void
test_version_dotted_vs_hyphenated(void)
{
	TEST_ASSERT_EQ(pkgconf_compare_version("1.0-1", "1.0.1"), 0);
}

static void
test_version_leading_zeros(void)
{
	TEST_ASSERT_EQ(pkgconf_compare_version("1.01", "1.1"), 0);
	TEST_ASSERT_EQ(pkgconf_compare_version("01.0", "1.0"), 0);
}

static void
test_version_trailing_zero_segments(void)
{
	/* Pkgconf does NOT treat "1.0" and "1.0.0" as equivalent. 
	 * Even a zero trailing numeric segment is additional content that makes the version greater.
	 */
	TEST_ASSERT_LT(pkgconf_compare_version("1.0", "1.0.0"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("1.0.0", "1.0"), 0);
	TEST_ASSERT_LT(pkgconf_compare_version("1", "1.0"), 0);
	TEST_ASSERT_LT(pkgconf_compare_version("1", "1.0.0.0"), 0);
}

static void
test_version_null_handling(void)
{
	TEST_ASSERT_LT(pkgconf_compare_version(NULL, "1.0"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("1.0", NULL), 0);
	TEST_ASSERT_LT(pkgconf_compare_version(NULL, NULL), 0);
}

static void
test_version_tilde_both_sides(void)
{
	TEST_ASSERT_LT(pkgconf_compare_version("1.0~", "1.0a"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("1.0a", "1.0~"), 0);

	TEST_ASSERT_EQ(pkgconf_compare_version("1.0~rc", "1.0~rc"), 0);
	TEST_ASSERT_LT(pkgconf_compare_version("1.0~a", "1.0~b"), 0);
	TEST_ASSERT_LT(pkgconf_compare_version("1.0~1", "1.0~2"), 0);
}

static void
test_version_separator_equivalence(void)
{
	TEST_ASSERT_EQ(pkgconf_compare_version("1.0", "1-0"), 0);
	TEST_ASSERT_EQ(pkgconf_compare_version("1.2.3", "1_2_3"), 0);
	TEST_ASSERT_EQ(pkgconf_compare_version("1..2", "1.2"), 0);
}

static void
test_version_case_insensitive(void)
{
	TEST_ASSERT_EQ(pkgconf_compare_version("1.0RC1", "1.0rc1"), 0);
	TEST_ASSERT_EQ(pkgconf_compare_version("1.0A", "1.0a"), 0);
}

static void
test_version_alpha_prefix(void)
{
	TEST_ASSERT_LT(pkgconf_compare_version("1.0alpha", "1.0alphabeta"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("1.0alphabeta", "1.0alpha"), 0);

	TEST_ASSERT_LT(pkgconf_compare_version("1.0a", "1.0ab"), 0);
	TEST_ASSERT_GT(pkgconf_compare_version("1.0ab", "1.0a"), 0);
}

int
main(int argc, char *argv[])
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_comparator_lookup_known);
	TEST_RUN(basename, test_comparator_lookup_unknown);
	TEST_RUN(basename, test_comparator_roundtrip);

	TEST_RUN(basename, test_parse_str_empty);
	TEST_RUN(basename, test_parse_str_single);
	TEST_RUN(basename, test_parse_str_versioned);
	TEST_RUN(basename, test_parse_str_multiple_space_separated);
	TEST_RUN(basename, test_parse_str_multiple_comma_separated);
	TEST_RUN(basename, test_parse_str_mixed_versioned_and_bare);
	TEST_RUN(basename, test_parse_str_all_comparators);

	TEST_RUN(basename, test_dependency_add);
	TEST_RUN(basename, test_dependency_add_no_version);
	TEST_RUN(basename, test_dependency_add_multiple);
	TEST_RUN(basename, test_dependency_collision_drops_flagged_newcomer);
	TEST_RUN(basename, test_dependency_collision_drops_flagged_existing);

	TEST_RUN(basename, test_version_equal);
	TEST_RUN(basename, test_version_simple_numeric);
	TEST_RUN(basename, test_version_numeric_segments_not_lexical);
	TEST_RUN(basename, test_version_different_lengths);
	TEST_RUN(basename, test_version_alpha_suffix);
	TEST_RUN(basename, test_version_tilde_prerelease);
	TEST_RUN(basename, test_version_numeric_beats_alpha);
	TEST_RUN(basename, test_version_alpha_ordering);
	TEST_RUN(basename, test_version_dotted_vs_hyphenated);
	TEST_RUN(basename, test_version_leading_zeros);
	TEST_RUN(basename, test_version_trailing_zero_segments);
	TEST_RUN(basename, test_version_null_handling);
	TEST_RUN(basename, test_version_tilde_both_sides);
	TEST_RUN(basename, test_version_separator_equivalence);
	TEST_RUN(basename, test_version_case_insensitive);
	TEST_RUN(basename, test_version_alpha_prefix);

	return EXIT_SUCCESS;
}
