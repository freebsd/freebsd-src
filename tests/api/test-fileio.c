/*
 * test-fileio.c
 * Tests for the public libpkgconf file i/o API.
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

static FILE *
fmemstream(const char *contents)
{
	FILE *f = tmpfile();

	TEST_ASSERT_NONNULL(f);

	fwrite(contents, 1, strlen(contents), f);
	rewind(f);

	return f;
}

static void
test_fgetline_no_trailing_newline(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	FILE *f = fmemstream("hello");

	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "hello");

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_FALSE(pkgconf_fgetline(&buf, f));

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

static void
test_fgetline_empty_stream(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	FILE *f = fmemstream("");

	TEST_ASSERT_FALSE(pkgconf_fgetline(&buf, f));

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

static void
test_fgetline_lf(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	FILE *f = fmemstream("hello\nworld\n");

	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "hello");

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "world");

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_FALSE(pkgconf_fgetline(&buf, f));

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

static void
test_fgetline_crlf(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	FILE *f = fmemstream("hello\r\nworld\r\n");

	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "hello");

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "world");

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_FALSE(pkgconf_fgetline(&buf, f));

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

static void
test_fgetline_lone_cr(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	FILE *f = fmemstream("hello\rworld\r");

	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "hello");

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "world");

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_FALSE(pkgconf_fgetline(&buf, f));

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

static void
test_fgetline_backslash_continuation_lf(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	FILE *f = fmemstream("foo\\\nbar\n");

	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "foobar");

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

static void
test_fgetline_backslash_continuation_crlf(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	FILE *f = fmemstream("foo\\\r\nbar\r\n");

	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "foobar");

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_FALSE(pkgconf_fgetline(&buf, f));

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

static void
test_fgetline_backslash_continuation_lone_cr(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	FILE *f = fmemstream("foo\\\rbar\r");

	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "foobar");

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_FALSE(pkgconf_fgetline(&buf, f));

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

// A backslash not immediately followed by a newline is not a continuation,
// so it must be preserved literally in the output.
static void
test_fgetline_backslash_not_continuation(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	FILE *f = fmemstream("foo\\bar\n");

	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "foo\\bar");

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_FALSE(pkgconf_fgetline(&buf, f));

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

// fgets() only stops on '\n', a full read buffer, or EOF. NOT on on a lone '\r'.
// If a '\r' happens to be the very last byte fgets() manages to read
// before the buffer fills up, the matching '\n' is still unread in the
// stream and isn't visible to pkgconf_fgetline()'s lookahead, so the CRLF
// pair gets split across two fgets() calls and is misparsed as two lines.
static void
test_fgetline_crlf_split_across_fgets_buffer(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	size_t prefix_len = PKGCONF_ITEM_SIZE - 2;
	char *content = malloc(prefix_len + strlen("\r\nworld\n") + 1);
	FILE *f;

	TEST_ASSERT_NONNULL(content);
	memset(content, 'a', prefix_len);
	strcpy(content + prefix_len, "\r\nworld\n");

	f = fmemstream(content);
	free(content);

	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_EQ(strlen(pkgconf_buffer_str(&buf)), prefix_len);

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_TRUE(pkgconf_fgetline(&buf, f));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "world");

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_FALSE(pkgconf_fgetline(&buf, f));

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

int
main(int argc, const char **argv)
{
	(void) argc;
	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_fgetline_no_trailing_newline);
	TEST_RUN(basename, test_fgetline_empty_stream);
	TEST_RUN(basename, test_fgetline_lf);
	TEST_RUN(basename, test_fgetline_crlf);
	TEST_RUN(basename, test_fgetline_lone_cr);
	TEST_RUN(basename, test_fgetline_backslash_continuation_lf);
	TEST_RUN(basename, test_fgetline_backslash_continuation_crlf);
	TEST_RUN(basename, test_fgetline_backslash_continuation_lone_cr);
	TEST_RUN(basename, test_fgetline_backslash_not_continuation);
	TEST_RUN(basename, test_fgetline_crlf_split_across_fgets_buffer);

	return EXIT_SUCCESS;
}
