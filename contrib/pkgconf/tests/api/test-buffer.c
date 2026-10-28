/*
 * test-buffer.c
 * Tests for libpkgconf buffer API.
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2025 pkgconf authors (see AUTHORS).
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

static void
test_buffer_empty(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	TEST_ASSERT_EQ(pkgconf_buffer_len(&buf), 0);
	TEST_ASSERT_NULL(pkgconf_buffer_str(&buf));
	TEST_ASSERT_EMPTY_STRING(pkgconf_buffer_str_or_empty(&buf));
	TEST_ASSERT_EQ(pkgconf_buffer_lastc(&buf), '\0');

	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_append(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	TEST_ASSERT_TRUE(pkgconf_buffer_append(&buf, "hello"));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "hello");
	TEST_ASSERT_EQ(pkgconf_buffer_len(&buf), 5);

	TEST_ASSERT_TRUE(pkgconf_buffer_append(&buf, " world"));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "hello world");
	TEST_ASSERT_EQ(pkgconf_buffer_len(&buf), 11);

	TEST_ASSERT_EQ(pkgconf_buffer_lastc(&buf), 'd');

	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_append_slice(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	TEST_ASSERT_TRUE(pkgconf_buffer_append_slice(&buf, "abcdefgh", 3));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "abc");

	TEST_ASSERT_TRUE(pkgconf_buffer_append_slice(&buf, "xyz", 0));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "abc");

	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_append_fmt(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	TEST_ASSERT_TRUE(pkgconf_buffer_append_fmt(&buf, "%s=%d", "x", 42));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "x=42");

	TEST_ASSERT_TRUE(pkgconf_buffer_append_fmt(&buf, " %s", "ok"));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "x=42 ok");

	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_prepend(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&buf, "world");
	TEST_ASSERT_TRUE(pkgconf_buffer_prepend(&buf, "hello "));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "hello world");

	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_push_byte(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	TEST_ASSERT_TRUE(pkgconf_buffer_push_byte(&buf, 'a'));
	TEST_ASSERT_TRUE(pkgconf_buffer_push_byte(&buf, 'b'));
	TEST_ASSERT_TRUE(pkgconf_buffer_push_byte(&buf, 'c'));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "abc");
	TEST_ASSERT_EQ(pkgconf_buffer_len(&buf), 3);

	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_trim_byte(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&buf, "hello\n");
	TEST_ASSERT_TRUE(pkgconf_buffer_trim_byte(&buf));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "hello");

	pkgconf_buffer_reset(&buf);
	pkgconf_buffer_push_byte(&buf, 'x');
	TEST_ASSERT_TRUE(pkgconf_buffer_trim_byte(&buf));
	TEST_ASSERT_EQ(pkgconf_buffer_len(&buf), 0);
	TEST_ASSERT_FALSE(pkgconf_buffer_trim_byte(&buf));

	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_reset(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&buf, "some content");
	TEST_ASSERT_NE(pkgconf_buffer_len(&buf), 0);

	pkgconf_buffer_reset(&buf);
	TEST_ASSERT_EQ(pkgconf_buffer_len(&buf), 0);
	TEST_ASSERT_NULL(pkgconf_buffer_str(&buf));

	TEST_ASSERT_TRUE(pkgconf_buffer_append(&buf, "fresh"));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "fresh");

	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_freeze(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&buf, "frozen");
	char *out = pkgconf_buffer_freeze(&buf);
	TEST_ASSERT_NONNULL(out);
	TEST_ASSERT_STRCMP_EQ(out, "frozen");
	TEST_ASSERT_EQ(pkgconf_buffer_len(&buf), 0);

	TEST_ASSERT_NULL(pkgconf_buffer_freeze(&buf));

	free(out);
	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_copy(void)
{
	pkgconf_buffer_t src = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t dst = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&src, "original");
	pkgconf_buffer_append(&dst, "to be replaced");
	TEST_ASSERT_TRUE(pkgconf_buffer_copy(&src, &dst));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&dst), "original");
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&src), "original"); // src is unchanged

	pkgconf_buffer_finalize(&src);
	pkgconf_buffer_finalize(&dst);
}

static void
test_buffer_rejects_aliasing(void)
{
	static const pkgconf_span_t spans[] = {
		{ 'a', 'z' },
	};
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	TEST_ASSERT_TRUE(pkgconf_buffer_append(&buf, "abc"));
	TEST_ASSERT_FALSE(pkgconf_buffer_append(&buf, pkgconf_buffer_str(&buf)));
	TEST_ASSERT_FALSE(pkgconf_buffer_append_slice(&buf, pkgconf_buffer_str(&buf), 1));
	TEST_ASSERT_FALSE(pkgconf_buffer_prepend(&buf, pkgconf_buffer_str(&buf)));
	TEST_ASSERT_FALSE(pkgconf_buffer_copy(&buf, &buf));
	TEST_ASSERT_FALSE(pkgconf_buffer_subst(&buf, &buf, "a", "x"));
	TEST_ASSERT_FALSE(pkgconf_buffer_escape(&buf, &buf, spans, PKGCONF_ARRAY_SIZE(spans)));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "abc");

	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_join(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	TEST_ASSERT_TRUE(pkgconf_buffer_join(&buf, '/', "usr", "local", "lib", NULL));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&buf), "usr/local/lib");

	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_contains(void)
{
	pkgconf_buffer_t hay = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t needle = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&hay, "the quick brown fox");

	pkgconf_buffer_append(&needle, "quick");
	TEST_ASSERT_TRUE(pkgconf_buffer_contains(&hay, &needle));

	pkgconf_buffer_reset(&needle);
	pkgconf_buffer_append(&needle, "slow");
	TEST_ASSERT_FALSE(pkgconf_buffer_contains(&hay, &needle));

	TEST_ASSERT_TRUE(pkgconf_buffer_contains_byte(&hay, 'q'));
	TEST_ASSERT_FALSE(pkgconf_buffer_contains_byte(&hay, 'z'));

	pkgconf_buffer_finalize(&hay);
	pkgconf_buffer_finalize(&needle);
}

static void
test_buffer_match(void)
{
	pkgconf_buffer_t a = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t b = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&a, "identical");
	pkgconf_buffer_append(&b, "identical");
	TEST_ASSERT_TRUE(pkgconf_buffer_match(&a, &b));

	// Identical length, but different contents
	pkgconf_buffer_reset(&b);
	pkgconf_buffer_append(&b, "different");
	TEST_ASSERT_FALSE(pkgconf_buffer_match(&a, &b));

	// Different length and contents
	pkgconf_buffer_reset(&b);
	pkgconf_buffer_append(&b, "different!");
	TEST_ASSERT_FALSE(pkgconf_buffer_match(&a, &b));

	pkgconf_buffer_finalize(&a);
	pkgconf_buffer_finalize(&b);
}

static void
test_buffer_has_prefix(void)
{
	pkgconf_buffer_t hay = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t pre = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&hay, "/usr/local/lib");

	pkgconf_buffer_append(&pre, "/usr");
	TEST_ASSERT_TRUE(pkgconf_buffer_has_prefix(&hay, &pre));

	pkgconf_buffer_reset(&pre);
	pkgconf_buffer_append(&pre, "/opt");
	TEST_ASSERT_FALSE(pkgconf_buffer_has_prefix(&hay, &pre));

	pkgconf_buffer_finalize(&hay);
	pkgconf_buffer_finalize(&pre);
}

static void
test_buffer_subst(void)
{
	pkgconf_buffer_t src = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t dst = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&src, "prefix=${PREFIX}/share");
	TEST_ASSERT_TRUE(pkgconf_buffer_subst(&dst, &src, "${PREFIX}", "/opt/foo"));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&dst), "prefix=/opt/foo/share");

	pkgconf_buffer_finalize(&src);
	pkgconf_buffer_finalize(&dst);
}

static void
test_buffer_subst_empty_pattern(void)
{
	pkgconf_buffer_t src = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t dst0 = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t dst1 = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t dst2 = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t dst3 = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t dst4 = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t dst5 = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&src, "prefix=${PREFIX}/share");

	TEST_ASSERT_TRUE(pkgconf_buffer_subst(&dst0, &src, "", "foo"));
	TEST_ASSERT_TRUE(pkgconf_buffer_subst(&dst1, &src, "", ""));
	TEST_ASSERT_TRUE(pkgconf_buffer_subst(&dst2, &src, "", NULL));
	TEST_ASSERT_TRUE(pkgconf_buffer_subst(&dst3, &src, NULL, "foo"));
	TEST_ASSERT_TRUE(pkgconf_buffer_subst(&dst4, &src, NULL, ""));
	TEST_ASSERT_TRUE(pkgconf_buffer_subst(&dst5, &src, NULL, NULL));

	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&dst0), "prefix=${PREFIX}/share");
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&dst1), "prefix=${PREFIX}/share");
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&dst2), "prefix=${PREFIX}/share");
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&dst3), "prefix=${PREFIX}/share");
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&dst4), "prefix=${PREFIX}/share");
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&dst5), "prefix=${PREFIX}/share");

	pkgconf_buffer_finalize(&src);
	pkgconf_buffer_finalize(&dst0);
	pkgconf_buffer_finalize(&dst1);
	pkgconf_buffer_finalize(&dst2);
	pkgconf_buffer_finalize(&dst3);
	pkgconf_buffer_finalize(&dst4);
	pkgconf_buffer_finalize(&dst5);
}

static void
test_buffer_escape(void)
{
	pkgconf_buffer_t src = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t dst = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_span_t spans[] = {
		{ ' ', ' ' },
		{ '\t', '\t' },
	};

	pkgconf_buffer_append(&src, "a b\tc");
	TEST_ASSERT_TRUE(pkgconf_buffer_escape(&dst, &src, spans, PKGCONF_ARRAY_SIZE(spans)));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&dst), "a\\ b\\\tc");

	pkgconf_buffer_finalize(&src);
	pkgconf_buffer_finalize(&dst);
}

static void
test_buffer_escape_charset(void)
{
	pkgconf_buffer_t src = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t dst = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_charset_t charset;

	static const pkgconf_span_t spans[] = {
		{ ' ', ' ' },
		{ '\t', '\t' },
	};

	pkgconf_charset_from_spans(&charset, spans, PKGCONF_ARRAY_SIZE(spans));

	pkgconf_buffer_append(&src, "a b\tc");
	TEST_ASSERT_TRUE(pkgconf_buffer_escape_charset(&dst, &src, &charset));
	TEST_ASSERT_STRCMP_EQ(pkgconf_buffer_str(&dst), "a\\ b\\\tc");

	/* escaping into the source buffer must still be rejected */
	TEST_ASSERT_FALSE(pkgconf_buffer_escape_charset(&src, &src, &charset));

	pkgconf_buffer_finalize(&src);
	pkgconf_buffer_finalize(&dst);
}

/*
 * A charset is built a 64-bit word at a time, so every combination of span
 * endpoints relative to a word boundary needs to agree with a plain span walk.
 * Check each such combination, including lo > hi (an empty span).
 */
static void
test_charset_word_boundaries(void)
{
	/* every boundary class: word start/end, either side of one, and the extremes */
	static const unsigned char interesting[] = {
		0x00, 0x01, 0x3e, 0x3f, 0x40, 0x41, 0x7e, 0x7f,
		0x80, 0x81, 0xbe, 0xbf, 0xc0, 0xc1, 0xfe, 0xff,
	};

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(interesting); i++)
	{
		for (size_t j = 0; j < PKGCONF_ARRAY_SIZE(interesting); j++)
		{
			pkgconf_charset_t charset;
			const pkgconf_span_t span = { interesting[i], interesting[j] };

			pkgconf_charset_from_spans(&charset, &span, 1);

			for (unsigned int c = 0; c <= 0xff; c++)
			{
				TEST_ASSERT_EQ(pkgconf_charset_contains(&charset, (unsigned char) c),
					pkgconf_span_contains((unsigned char) c, &span, 1));
			}
		}
	}
}

/*
 * A charset must agree with a span walk for every one of the 256 byte values,
 * since it exists purely to hoist that walk out of per-byte loops.  Covers the
 * span sets fragment quoting uses plus the awkward edges: a span reaching 0xff,
 * an empty set, and lo > hi.
 */
static void
test_charset_matches_spans(void)
{
	static const pkgconf_span_t quote_spans[] = {
		{ 0x00, 0x1f }, { ' ', '#' }, { '%', '\'' }, { '*', '*' }, { ';', '<' },
		{ '>', '?' }, { '[', ']' }, { '`', '`' }, { '{', '}' }, { 0x7f, 0xff },
	};
	static const pkgconf_span_t quote_spans_utf8[] = {
		{ 0x00, 0x1f }, { ' ', '#' }, { '%', '\'' }, { '*', '*' }, { ';', '<' },
		{ '>', '?' }, { '[', ']' }, { '`', '`' }, { '{', '}' }, { 0x7f, 0x7f },
	};
	static const pkgconf_span_t boundaries[] = {
		{ 0xff, 0xff }, { 0x00, 0x00 }, { 0x3f, 0x40 }, { 0x40, 0x41 },
	};
	static const pkgconf_span_t everything[] = { { 0x00, 0xff } };
	static const pkgconf_span_t inverted[] = { { 0x80, 0x7f } };
	/* spans which each cover exactly one word, and one which straddles all four */
	static const pkgconf_span_t words[] = {
		{ 0x00, 0x3f }, { 0x40, 0x7f }, { 0x80, 0xbf }, { 0xc0, 0xff },
	};
	static const pkgconf_span_t straddle[] = { { 0x01, 0xfe } };

	const struct {
		const pkgconf_span_t *spans;
		size_t nspans;
	} cases[] = {
		{ quote_spans, PKGCONF_ARRAY_SIZE(quote_spans) },
		{ quote_spans_utf8, PKGCONF_ARRAY_SIZE(quote_spans_utf8) },
		{ boundaries, PKGCONF_ARRAY_SIZE(boundaries) },
		{ everything, PKGCONF_ARRAY_SIZE(everything) },
		{ inverted, PKGCONF_ARRAY_SIZE(inverted) },
		{ words, PKGCONF_ARRAY_SIZE(words) },
		{ words, 1 },
		{ words + 1, 1 },
		{ words + 2, 1 },
		{ words + 3, 1 },
		{ straddle, PKGCONF_ARRAY_SIZE(straddle) },
		{ quote_spans, 0 },
	};

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(cases); i++)
	{
		pkgconf_charset_t charset;

		pkgconf_charset_from_spans(&charset, cases[i].spans, cases[i].nspans);

		for (unsigned int c = 0; c <= 0xff; c++)
		{
			TEST_ASSERT_EQ(pkgconf_charset_contains(&charset, (unsigned char) c),
				pkgconf_span_contains((unsigned char) c, cases[i].spans, cases[i].nspans));
		}
	}
}

static void
test_str_eq_slice(void)
{
	TEST_ASSERT_TRUE(pkgconf_str_eq_slice("hello", "hello", 5));
	TEST_ASSERT_FALSE(pkgconf_str_eq_slice("hello!", "hello", 5));
	TEST_ASSERT_FALSE(pkgconf_str_eq_slice("hello", "world", 5));
	TEST_ASSERT_FALSE(pkgconf_str_eq_slice(NULL, "hello", 5));
}

static void
test_span_contains(void)
{
	pkgconf_span_t spans[] = {
		{ '0', '9' },
		{ 'a', 'z' },
	};

	TEST_ASSERT_TRUE(pkgconf_span_contains('5', spans, PKGCONF_ARRAY_SIZE(spans)));
	TEST_ASSERT_TRUE(pkgconf_span_contains('m', spans, PKGCONF_ARRAY_SIZE(spans)));
	TEST_ASSERT_FALSE(pkgconf_span_contains('A', spans, PKGCONF_ARRAY_SIZE(spans)));
	TEST_ASSERT_FALSE(pkgconf_span_contains('!', spans, PKGCONF_ARRAY_SIZE(spans)));
}

static void
test_buffer_fputs_nonempty(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	FILE *f = tmpfile();
	TEST_ASSERT_NONNULL(f);

	pkgconf_buffer_append(&buf, "hello world");
	TEST_ASSERT_TRUE(pkgconf_buffer_fputs(&buf, f));

	char out[64];
	rewind(f);
	size_t n = fread(out, 1, sizeof(out) - 1, f);
	out[n] = '\0';

	// Buffer contents followed by a newline.
	TEST_ASSERT_STRCMP_EQ(out, "hello world\n");

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

static void
test_buffer_fputs_empty(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	FILE *f = tmpfile();
	TEST_ASSERT_NONNULL(f);

	// An empty buffer writes only a newline.
	TEST_ASSERT_TRUE(pkgconf_buffer_fputs(&buf, f));

	char out[64];
	rewind(f);
	size_t n = fread(out, 1, sizeof(out) - 1, f);
	out[n] = '\0';

	TEST_ASSERT_STRCMP_EQ(out, "\n");

	fclose(f);
	pkgconf_buffer_finalize(&buf);
}

int
main(int argc, const char **argv)
{
	(void) argc;

	const char *basename = pkgconf_path_find_basename(argv[0]);

	TEST_RUN(basename, test_buffer_empty);
	TEST_RUN(basename, test_buffer_append);
	TEST_RUN(basename, test_buffer_append_slice);
	TEST_RUN(basename, test_buffer_append_fmt);
	TEST_RUN(basename, test_buffer_prepend);
	TEST_RUN(basename, test_buffer_push_byte);
	TEST_RUN(basename, test_buffer_trim_byte);
	TEST_RUN(basename, test_buffer_reset);
	TEST_RUN(basename, test_buffer_freeze);
	TEST_RUN(basename, test_buffer_copy);
	TEST_RUN(basename, test_buffer_rejects_aliasing);
	TEST_RUN(basename, test_buffer_join);
	TEST_RUN(basename, test_buffer_contains);
	TEST_RUN(basename, test_buffer_match);
	TEST_RUN(basename, test_buffer_has_prefix);
	TEST_RUN(basename, test_buffer_subst);
	TEST_RUN(basename, test_buffer_subst_empty_pattern);
	TEST_RUN(basename, test_buffer_escape);
	TEST_RUN(basename, test_buffer_escape_charset);
	TEST_RUN(basename, test_charset_matches_spans);
	TEST_RUN(basename, test_charset_word_boundaries);
	TEST_RUN(basename, test_str_eq_slice);
	TEST_RUN(basename, test_span_contains);
	TEST_RUN(basename, test_buffer_fputs_nonempty);
	TEST_RUN(basename, test_buffer_fputs_empty);

	return EXIT_SUCCESS;
}
