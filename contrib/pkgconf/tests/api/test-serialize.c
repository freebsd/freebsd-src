/*
 * test-serialize.c
 * Tests for spdxtool's JSON serialization value model.
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

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include "test-api.h"
#include "serialize.h"

// Render a value to a freshly-allocated C string (caller frees)
static char *
render(spdxtool_serialize_value_t *value)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	TEST_ASSERT_TRUE(spdxtool_serialize_value_to_buf(&buf, value, 0));
	char *s = strdup(pkgconf_buffer_str_or_empty(&buf));
	pkgconf_buffer_finalize(&buf);
	return s;
}

// Render a container without taking ownership of it
static char *
render_object(spdxtool_serialize_object_list_t *o)
{
	spdxtool_serialize_value_t wrap = { .type = SPDXTOOL_SERIALIZE_TYPE_OBJECT, .value = { .o = o } };
	return render(&wrap);
}

static char *
render_array(spdxtool_serialize_array_t *a)
{
	spdxtool_serialize_value_t wrap = { .type = SPDXTOOL_SERIALIZE_TYPE_ARRAY, .value = { .a = a } };
	return render(&wrap);
}

// Scalar value types: string / int / bool / null
static void
test_serialize_value_string(void)
{
	spdxtool_serialize_value_t *v = spdxtool_serialize_value_string("hi");
	TEST_ASSERT_NONNULL(v);
	char *s = render(v);
	TEST_ASSERT_STRCMP_EQ(s, "\"hi\"");
	free(s);
	spdxtool_serialize_value_free(v);
}

static void
test_serialize_value_int(void)
{
	spdxtool_serialize_value_t *v = spdxtool_serialize_value_int(123);
	TEST_ASSERT_NONNULL(v);
	char *s = render(v);
	TEST_ASSERT_STRCMP_EQ(s, "123");
	free(s);
	spdxtool_serialize_value_free(v);
}

static void
test_serialize_value_bool(void)
{
	spdxtool_serialize_value_t *t = spdxtool_serialize_value_bool(true);
	spdxtool_serialize_value_t *f = spdxtool_serialize_value_bool(false);
	char *st = render(t);
	char *sf = render(f);
	TEST_ASSERT_STRCMP_EQ(st, "true");
	TEST_ASSERT_STRCMP_EQ(sf, "false");
	free(st);
	free(sf);
	spdxtool_serialize_value_free(t);
	spdxtool_serialize_value_free(f);
}

static void
test_serialize_value_null(void)
{
	spdxtool_serialize_value_t *v = spdxtool_serialize_value_null();
	TEST_ASSERT_NONNULL(v);
	char *s = render(v);
	TEST_ASSERT_STRCMP_EQ(s, "null");
	free(s);
	spdxtool_serialize_value_free(v);
}

// All JSON string escape sequences, including control characters
static void
test_serialize_escape_sequences(void)
{
	spdxtool_serialize_value_t *v = spdxtool_serialize_value_string("\"\\\b\f\n\r\t\x01" "Z");
	char *s = render(v);

	TEST_ASSERT_STRSTR(s, "\\\"");     // quote
	TEST_ASSERT_STRSTR(s, "\\\\");     // backslash
	TEST_ASSERT_STRSTR(s, "\\b");      // backspace
	TEST_ASSERT_STRSTR(s, "\\f");      // form feed
	TEST_ASSERT_STRSTR(s, "\\n");      // newline
	TEST_ASSERT_STRSTR(s, "\\r");      // carriage return
	TEST_ASSERT_STRSTR(s, "\\t");      // tab
	TEST_ASSERT_STRSTR(s, "\\u0001");  // other ctrl
	TEST_ASSERT_STRSTR(s, "Z");        // printable passthrough

	free(s);
	spdxtool_serialize_value_free(v);
}

// Mixed-type object exercises the int/bool/null add helpers
static void
test_serialize_object_mixed_types(void)
{
	spdxtool_serialize_object_list_t *o = spdxtool_serialize_object_list_new();
	TEST_ASSERT_NONNULL(o);

	TEST_ASSERT_NONNULL(spdxtool_serialize_object_add_string(o, "s", "x"));
	TEST_ASSERT_NONNULL(spdxtool_serialize_object_add_int(o, "i", 42));
	TEST_ASSERT_NONNULL(spdxtool_serialize_object_add_bool(o, "b", true));
	TEST_ASSERT_NONNULL(spdxtool_serialize_object_add_null(o, "n"));

	char *s = render_object(o);
	TEST_ASSERT_STRSTR(s, "\"s\": \"x\"");
	TEST_ASSERT_STRSTR(s, "\"i\": 42");
	TEST_ASSERT_STRSTR(s, "\"b\": true");
	TEST_ASSERT_STRSTR(s, "\"n\": null");

	free(s);
	spdxtool_serialize_object_list_free(o);
}

// Mixed-type array exercises the int/bool/null array helpers
static void
test_serialize_array_mixed_types(void)
{
	spdxtool_serialize_array_t *a = spdxtool_serialize_array_new();
	TEST_ASSERT_NONNULL(a);

	TEST_ASSERT_NONNULL(spdxtool_serialize_array_add_int(a, 7));
	TEST_ASSERT_NONNULL(spdxtool_serialize_array_add_bool(a, false));
	TEST_ASSERT_NONNULL(spdxtool_serialize_array_add_null(a));

	char *s = render_array(a);
	TEST_ASSERT_STRSTR(s, "7");
	TEST_ASSERT_STRSTR(s, "false");
	TEST_ASSERT_STRSTR(s, "null");

	free(s);
	spdxtool_serialize_array_free(a);
}

// Defensive NULL handling that the CLI path never reaches
static void
test_serialize_null_guards(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	spdxtool_serialize_value_t *v = spdxtool_serialize_value_null();

	// value_to_buf rejects NULL buffer or NULL value
	TEST_ASSERT_FALSE(spdxtool_serialize_value_to_buf(NULL, v, 0));
	TEST_ASSERT_FALSE(spdxtool_serialize_value_to_buf(&buf, NULL, 0));
	pkgconf_buffer_finalize(&buf);

	// value_string(NULL) yields NULL
	TEST_ASSERT_NULL(spdxtool_serialize_value_string(NULL));

	// add_take with a NULL container frees the value and returns NULL
	TEST_ASSERT_NULL(spdxtool_serialize_object_add_take(NULL, "k", v));
	TEST_ASSERT_NULL(spdxtool_serialize_array_add_take(NULL, spdxtool_serialize_value_null()));

	// free routines are NULL-safe
	spdxtool_serialize_value_free(NULL);
	spdxtool_serialize_object_free(NULL);
	spdxtool_serialize_object_list_free(NULL);
	spdxtool_serialize_array_free(NULL);
}

int
main(int argc, const char **argv)
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_serialize_value_string);
	TEST_RUN(basename, test_serialize_value_int);
	TEST_RUN(basename, test_serialize_value_bool);
	TEST_RUN(basename, test_serialize_value_null);
	TEST_RUN(basename, test_serialize_escape_sequences);
	TEST_RUN(basename, test_serialize_object_mixed_types);
	TEST_RUN(basename, test_serialize_array_mixed_types);
	TEST_RUN(basename, test_serialize_null_guards);

	return EXIT_SUCCESS;
}
