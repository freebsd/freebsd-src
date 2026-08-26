/*
 * test-api.h
 * test API macros and functions.
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

#ifndef TEST_API_H
#define TEST_API_H

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include <libpkgconf/path.h>
#include <tests/win-shim.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wpragmas" // older gcc may not know the next one
	// Stop false-positive warnings for test cases
#	pragma GCC diagnostic ignored "-Wanalyzer-tainted-assertion"
#endif

#define TEST_FAIL_(fmt, ...)	\
	do {	\
		fprintf(stderr, "FAIL: %s:%d: " fmt "\n",	\
			__FILE__, __LINE__, __VA_ARGS__);	\
		exit(EXIT_FAILURE);	\
	} while (0)

#define TEST_ASSERT_TRUE(expr)	\
	do {	\
		if (!(expr))	\
			TEST_FAIL_("TEST_ASSERT_TRUE(%s) was false", #expr);	\
	} while (0)

#define TEST_ASSERT_FALSE(expr)	\
	do {	\
		if ((expr))	\
			TEST_FAIL_("TEST_ASSERT_FALSE(%s) was true", #expr);	\
	} while (0)

#define TEST_ASSERT_NULL(expr)	\
	do {	\
		const void *_v = (const void *)(expr);	\
		if (_v != NULL)	\
			TEST_FAIL_("TEST_ASSERT_NULL(%s) was %p", #expr, _v);	\
	} while (0)

#define TEST_ASSERT_NONNULL(expr)	\
	do {	\
		if ((expr) == NULL)	\
			TEST_FAIL_("TEST_ASSERT_NONNULL(%s) was NULL", #expr);	\
	} while (0)

#define TEST_ASSERT_EQ(a, b)	\
	do {	\
		long long _a = (long long)(a);	\
		long long _b = (long long)(b);	\
		if (_a != _b)	\
			TEST_FAIL_("TEST_ASSERT_EQ(%s, %s): %lld != %lld",	\
				#a, #b, _a, _b);	\
	} while (0)

#define TEST_ASSERT_LT(a, b)	\
	do {	\
		long long _a = (long long)(a);	\
		long long _b = (long long)(b);	\
		if (_a >= _b)	\
			TEST_FAIL_("TEST_ASSERT_LT(%s, %s): %lld >= %lld",	\
				#a, #b, _a, _b);	\
	} while (0)

#define TEST_ASSERT_LE(a, b)	\
	do {	\
		long long _a = (long long)(a);	\
		long long _b = (long long)(b);	\
		if (_a > _b)	\
			TEST_FAIL_("TEST_ASSERT_LE(%s, %s): %lld > %lld",	\
				#a, #b, _a, _b);	\
	} while (0)

#define TEST_ASSERT_GT(a, b)	\
	do {	\
		long long _a = (long long)(a);	\
		long long _b = (long long)(b);	\
		if (_a <= _b)	\
			TEST_FAIL_("TEST_ASSERT_GT(%s, %s): %lld <= %lld",	\
				#a, #b, _a, _b);	\
	} while (0)

#define TEST_ASSERT_GE(a, b)	\
	do {	\
		long long _a = (long long)(a);	\
		long long _b = (long long)(b);	\
		if (_a < _b)	\
			TEST_FAIL_("TEST_ASSERT_GE(%s, %s): %lld < %lld",	\
				#a, #b, _a, _b);	\
	} while (0)

#define TEST_ASSERT_NE(a, b)	\
	do {	\
		long long _a = (long long)(a);	\
		long long _b = (long long)(b);	\
		if (_a == _b)	\
			TEST_FAIL_("TEST_ASSERT_NE(%s, %s): both %lld",	\
				#a, #b, _a);	\
	} while (0)

#define TEST_ASSERT_STRCMP_EQ(actual, expected)	\
	do {	\
		const char *_a = (actual);	\
		const char *_e = (expected);	\
		if (_a == NULL || _e == NULL || strcmp(_a, _e) != 0)	\
			TEST_FAIL_("TEST_ASSERT_STRCMP_EQ(%s, %s): [%s] != [%s]",	\
				#actual, #expected,	\
				_a ? _a : "(null)", _e ? _e : "(null)");	\
	} while (0)

#define TEST_ASSERT_STRCASECMP_EQ(actual, expected)	\
	do {	\
		const char *_a = (actual);	\
		const char *_e = (expected);	\
		if (_a == NULL || _e == NULL || strcasecmp(_a, _e) != 0)	\
			TEST_FAIL_("TEST_ASSERT_STRCASECMP_EQ(%s, %s): [%s] !~ [%s]",	\
				#actual, #expected,	\
				_a ? _a : "(null)", _e ? _e : "(null)");	\
	} while (0)

#define TEST_ASSERT_STRNCMP_EQ(actual, expected, n)	\
	do {	\
		const char *_a = (actual);	\
		const char *_e = (expected);	\
		size_t _n = (size_t)(n);	\
		if (_a == NULL || _e == NULL || strncmp(_a, _e, _n) != 0)	\
			TEST_FAIL_("TEST_ASSERT_STRNCMP_EQ(%s, %s, %zu): [%s] != [%s]",	\
				#actual, #expected, _n,	\
				_a ? _a : "(null)", _e ? _e : "(null)");	\
	} while (0)

#define TEST_ASSERT_STRSTR(haystack, needle)	\
	do {	\
		const char *_h = (haystack);	\
		const char *_n = (needle);	\
		if (_h == NULL || _n == NULL || strstr(_h, _n) == NULL)	\
			TEST_FAIL_("TEST_ASSERT_STRSTR(%s, %s): [%s] does not contain [%s]",	\
				#haystack, #needle,	\
				_h ? _h : "(null)", _n ? _n : "(null)");	\
	} while (0)

#define TEST_ASSERT_EMPTY_STRING(expr)	\
	do {	\
		const char *_s = (expr);	\
		if (_s == NULL)	\
			TEST_FAIL_("TEST_ASSERT_EMPTY_STRING(%s) was NULL", #expr);	\
		if (_s[0] != '\0')	\
			TEST_FAIL_("TEST_ASSERT_EMPTY_STRING(%s) was [%s]", #expr, _s);	\
	} while (0)

#define TEST_RUN(name, fn)	\
	do {	\
		fprintf(stderr, "%s -> %s:", name, #fn);	\
		fn();	\
		fprintf(stderr, " PASS\n");	\
	} while (0)

static inline pkgconf_client_t *
test_client_new(void)
{
	pkgconf_cross_personality_t *pers = pkgconf_cross_personality_default();
	return pkgconf_client_new(NULL, NULL, pers, NULL, NULL);
}

#endif // TEST_API_H
