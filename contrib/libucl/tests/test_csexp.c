/*
 * Copyright (c) 2026, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ucl.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int failed = 0;

#define FAIL(fmt, ...) \
	do { \
		fprintf(stderr, "FAIL [%s]: " fmt "\n", __func__, ##__VA_ARGS__); \
		failed++; \
		return; \
	} while (0)

#define CHECK(cond, msg) \
	do { \
		if (!(cond)) { FAIL("%s", msg); } \
	} while (0)

/* Parse CSEXP input and return the top-level object (caller must unref). */
static ucl_object_t *
parse_ok(const unsigned char *data, size_t len, unsigned flags)
{
	struct ucl_parser *parser = ucl_parser_new(flags | UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj = NULL;
	bool ok;

	ok = ucl_parser_add_chunk_full(parser, data, len,
								   0, UCL_DUPLICATE_APPEND, UCL_PARSE_CSEXP);
	if (ok) {
		obj = ucl_parser_get_object(parser);
	}
	ucl_parser_free(parser);
	return obj;
}

/* Return true if parsing the input fails. */
static bool
parse_fails(const unsigned char *data, size_t len)
{
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	bool ok;

	ok = ucl_parser_add_chunk_full(parser, data, len,
								   0, UCL_DUPLICATE_APPEND, UCL_PARSE_CSEXP);
	ucl_parser_free(parser);
	return !ok;
}

/* ------------------------------------------------------------------ */
/* Valid-input tests                                                    */
/* ------------------------------------------------------------------ */

/* (3:abc) — single string element */
static void
test_single_element(void)
{
	const unsigned char input[] = {'(', '3', ':', 'a', 'b', 'c', ')'};
	ucl_object_t *top = parse_ok(input, sizeof(input), 0);

	CHECK(top != NULL, "parse returned NULL");
	CHECK(top->type == UCL_ARRAY, "top object is not an array");
	CHECK(ucl_array_size(top) == 1, "expected 1 element");

	const ucl_object_t *elem = ucl_array_head(top);
	CHECK(elem != NULL, "array head is NULL");
	CHECK(elem->type == UCL_STRING, "element is not a string");

	size_t slen = 0;
	const char *sv = ucl_object_tolstring(elem, &slen);
	CHECK(slen == 3, "string length mismatch");
	CHECK(memcmp(sv, "abc", 3) == 0, "string content mismatch");

	ucl_object_unref(top);
}

/* (11:hello world) — multi-digit length */
static void
test_multi_digit_length(void)
{
	const unsigned char input[] = {
		'(', '1', '1', ':', 'h', 'e', 'l', 'l', 'o', ' ',
		'w', 'o', 'r', 'l', 'd', ')'
	};
	ucl_object_t *top = parse_ok(input, sizeof(input), 0);

	CHECK(top != NULL, "parse returned NULL");
	CHECK(ucl_array_size(top) == 1, "expected 1 element");

	size_t slen = 0;
	const char *sv = ucl_object_tolstring(ucl_array_head(top), &slen);
	CHECK(slen == 11, "length mismatch");
	CHECK(memcmp(sv, "hello world", 11) == 0, "content mismatch");

	ucl_object_unref(top);
}

/* (3:abc3:def) — two elements in one list */
static void
test_multiple_elements(void)
{
	const unsigned char input[] = {
		'(', '3', ':', 'a', 'b', 'c', '3', ':', 'd', 'e', 'f', ')'
	};
	ucl_object_t *top = parse_ok(input, sizeof(input), 0);

	CHECK(top != NULL, "parse returned NULL");
	CHECK(ucl_array_size(top) == 2, "expected 2 elements");

	ucl_object_iter_t it = NULL;
	const ucl_object_t *cur;
	const char *expected[] = {"abc", "def"};
	int idx = 0;

	while ((cur = ucl_object_iterate(top, &it, true)) != NULL) {
		size_t slen = 0;
		const char *sv = ucl_object_tolstring(cur, &slen);
		if (slen != 3 || memcmp(sv, expected[idx], 3) != 0) {
			ucl_object_iterate_end(top, &it);
			ucl_object_unref(top);
			FAIL("element %d content mismatch", idx);
		}
		idx++;
	}
	ucl_object_iterate_end(top, &it);

	ucl_object_unref(top);
}

/* () — empty list */
static void
test_empty_list(void)
{
	const unsigned char input[] = {'(', ')'};
	ucl_object_t *top = parse_ok(input, sizeof(input), 0);

	CHECK(top != NULL, "parse returned NULL");
	CHECK(top->type == UCL_ARRAY, "top object is not an array");
	CHECK(ucl_array_size(top) == 0, "expected empty array");

	ucl_object_unref(top);
}

/* ((3:abc)(4:test)) — nested lists */
static void
test_nested_lists(void)
{
	const unsigned char input[] = {
		'(', '(', '3', ':', 'a', 'b', 'c', ')',
		'(', '4', ':', 't', 'e', 's', 't', ')', ')'
	};
	ucl_object_t *top = parse_ok(input, sizeof(input), 0);

	CHECK(top != NULL, "parse returned NULL");
	CHECK(top->type == UCL_ARRAY, "top is not an array");
	CHECK(ucl_array_size(top) == 2, "expected 2 nested arrays");

	const ucl_object_t *inner;

	inner = ucl_array_find_index(top, 0);
	CHECK(inner != NULL && inner->type == UCL_ARRAY, "first nested element is not array");
	CHECK(ucl_array_size(inner) == 1, "first nested array size mismatch");
	size_t slen = 0;
	const char *sv = ucl_object_tolstring(ucl_array_head(inner), &slen);
	CHECK(slen == 3 && memcmp(sv, "abc", 3) == 0, "first nested string mismatch");

	inner = ucl_array_find_index(top, 1);
	CHECK(inner != NULL && inner->type == UCL_ARRAY, "second nested element is not array");
	CHECK(ucl_array_size(inner) == 1, "second nested array size mismatch");
	sv = ucl_object_tolstring(ucl_array_head(inner), &slen);
	CHECK(slen == 4 && memcmp(sv, "test", 4) == 0, "second nested string mismatch");

	ucl_object_unref(top);
}

/* (3:\x00\x01\x02) — binary data containing null bytes */
static void
test_binary_data(void)
{
	const unsigned char input[] = {'(', '3', ':', '\x00', '\x01', '\x02', ')'};
	ucl_object_t *top = parse_ok(input, sizeof(input), 0);

	CHECK(top != NULL, "parse returned NULL");
	CHECK(ucl_array_size(top) == 1, "expected 1 element");

	const ucl_object_t *elem = ucl_array_head(top);
	CHECK(elem != NULL, "array head is NULL");
	CHECK(elem->flags & UCL_OBJECT_BINARY, "element missing BINARY flag");

	size_t slen = 0;
	const char *sv = ucl_object_tolstring(elem, &slen);
	CHECK(slen == 3, "binary length mismatch");
	CHECK((unsigned char) sv[0] == 0x00, "byte 0 mismatch");
	CHECK((unsigned char) sv[1] == 0x01, "byte 1 mismatch");
	CHECK((unsigned char) sv[2] == 0x02, "byte 2 mismatch");

	ucl_object_unref(top);
}

/* Same as test_single_element but with ZEROCOPY flag */
static void
test_zerocopy(void)
{
	const unsigned char input[] = {'(', '3', ':', 'a', 'b', 'c', ')'};
	ucl_object_t *top = parse_ok(input, sizeof(input), UCL_PARSER_ZEROCOPY);

	CHECK(top != NULL, "parse returned NULL");
	CHECK(ucl_array_size(top) == 1, "expected 1 element");

	size_t slen = 0;
	const char *sv = ucl_object_tolstring(ucl_array_head(top), &slen);
	CHECK(slen == 3, "zerocopy length mismatch");
	CHECK(memcmp(sv, "abc", 3) == 0, "zerocopy content mismatch");

	ucl_object_unref(top);
}

/* ((6:nested)) — doubly nested, single string */
static void
test_deeply_nested(void)
{
	const unsigned char input[] = {
		'(', '(', '6', ':', 'n', 'e', 's', 't', 'e', 'd', ')', ')'
	};
	ucl_object_t *top = parse_ok(input, sizeof(input), 0);

	CHECK(top != NULL, "parse returned NULL");
	CHECK(ucl_array_size(top) == 1, "outer array size mismatch");

	const ucl_object_t *inner = ucl_array_head(top);
	CHECK(inner != NULL && inner->type == UCL_ARRAY, "inner is not array");
	CHECK(ucl_array_size(inner) == 1, "inner array size mismatch");

	size_t slen = 0;
	const char *sv = ucl_object_tolstring(ucl_array_head(inner), &slen);
	CHECK(slen == 6 && memcmp(sv, "nested", 6) == 0, "nested string mismatch");

	ucl_object_unref(top);
}

/* ------------------------------------------------------------------ */
/* Error-input tests                                                    */
/* ------------------------------------------------------------------ */

/* (15:abc) — declared length exceeds available bytes (issue #361) */
static void
test_err_truncated(void)
{
	const unsigned char input[] = {'(', '1', '5', ':', 'a', 'b', 'c', ')'};
	CHECK(parse_fails(input, sizeof(input)), "should reject truncated data");
}

/* abc — bad starting character */
static void
test_err_bad_start(void)
{
	const unsigned char input[] = {'a', 'b', 'c'};
	CHECK(parse_fails(input, sizeof(input)), "should reject bad start char");
}

/* (a:abc) — non-numeric length */
static void
test_err_bad_length_char(void)
{
	const unsigned char input[] = {'(', 'a', ':', 'a', 'b', 'c', ')'};
	CHECK(parse_fails(input, sizeof(input)), "should reject non-numeric length");
}

/* (0:data) — zero-length prefix is rejected */
static void
test_err_zero_length(void)
{
	const unsigned char input[] = {'(', '0', ':', 'd', 'a', 't', 'a', ')'};
	CHECK(parse_fails(input, sizeof(input)), "should reject zero-length element");
}

/* (3:abc)) — extra closing brace */
static void
test_err_extra_brace(void)
{
	const unsigned char input[] = {'(', '3', ':', 'a', 'b', 'c', ')', ')'};
	CHECK(parse_fails(input, sizeof(input)), "should reject extra closing brace");
}

/* (3:abc — unclosed list */
static void
test_err_unclosed(void)
{
	const unsigned char input[] = {'(', '3', ':', 'a', 'b', 'c'};
	CHECK(parse_fails(input, sizeof(input)), "should reject unclosed list");
}

/* ------------------------------------------------------------------ */
/* Issue #365 regression tests                                         */
/* ------------------------------------------------------------------ */

/*
 * ()3:abc — value after outermost list is closed.
 * Before fix: NULL pointer dereference (parser->stack is NULL when
 * read_value tries to ucl_array_append).
 */
static void
test_err_value_after_close(void)
{
	const unsigned char input[] = {'(', ')', '3', ':', 'a', 'b', 'c'};
	CHECK(parse_fails(input, sizeof(input)),
		  "should reject value outside any list (issue #365 bug A)");
}

/*
 * (( — two unclosed lists.
 * Before fix: memory leak — inner stack object never appended to parent
 * and never freed. The parse must fail without leaking.
 */
static void
test_err_unclosed_nested(void)
{
	const unsigned char input[] = {'(', '('};
	CHECK(parse_fails(input, sizeof(input)),
		  "should reject unclosed nested lists without leaking (issue #365 bug B)");
}

/*
 * ((( — three unclosed lists, deeper nesting of the same leak pattern.
 */
static void
test_err_unclosed_deeply_nested(void)
{
	const unsigned char input[] = {'(', '(', '('};
	CHECK(parse_fails(input, sizeof(input)),
		  "should reject deeply unclosed lists without leaking");
}

/*
 * ()( — empty list followed by opening brace with no close.
 * Variant of bug A: after closing (), trailing ( pushes a new frame
 * but it never closes.
 */
static void
test_err_open_after_close(void)
{
	const unsigned char input[] = {'(', ')', '('};
	CHECK(parse_fails(input, sizeof(input)),
		  "should reject unclosed list after close");
}

/*
 * ((3:abc — inner element present but lists never closed.
 * The inner array has a child; verify the child is also freed.
 */
static void
test_err_unclosed_with_data(void)
{
	const unsigned char input[] = {'(', '(', '3', ':', 'a', 'b', 'c'};
	CHECK(parse_fails(input, sizeof(input)),
		  "should reject unclosed lists with data without leaking");
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	test_single_element();
	test_multi_digit_length();
	test_multiple_elements();
	test_empty_list();
	test_nested_lists();
	test_binary_data();
	test_zerocopy();
	test_deeply_nested();

	test_err_truncated();
	test_err_bad_start();
	test_err_bad_length_char();
	test_err_zero_length();
	test_err_extra_brace();
	test_err_unclosed();

	/* Issue #365 regression tests */
	test_err_value_after_close();
	test_err_unclosed_nested();
	test_err_unclosed_deeply_nested();
	test_err_open_after_close();
	test_err_unclosed_with_data();

	if (failed) {
		fprintf(stderr, "%d test(s) FAILED\n", failed);
		return 1;
	}

	printf("All CSEXP tests passed\n");
	return 0;
}
