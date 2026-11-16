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

/*
 * Tests for malformed/truncated msgpack input.
 * Regression tests for issue #363 (heap-buffer-overread in
 * ucl_msgpack_parse_ignore due to missing bounds check after length
 * reassignment for fixext/ext types).
 */

#include "ucl.h"
#include <stdio.h>
#include <string.h>

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

/* Return true if parsing the input as msgpack fails (does not crash). */
static bool
parse_msgpack_fails(const unsigned char *data, size_t len)
{
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	bool ok;

	ok = ucl_parser_add_chunk_full(parser, data, len,
								   0, UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK);
	ucl_parser_free(parser);
	return !ok;
}

/* Return true if parsing succeeds and produces a non-NULL object. */
static bool
parse_msgpack_ok(const unsigned char *data, size_t len)
{
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj = NULL;
	bool ok;

	ok = ucl_parser_add_chunk_full(parser, data, len,
								   0, UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK);
	if (ok) {
		obj = ucl_parser_get_object(parser);
	}
	ucl_parser_free(parser);

	if (obj) {
		ucl_object_unref(obj);
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------ */
/* Issue #363: fixext/ext types with insufficient remaining data       */
/* ------------------------------------------------------------------ */

/*
 * Original reproducer from the issue report:
 * fixarray(11) + fixext2 with only 2 bytes of data instead of 3.
 * 0x9b = fixarray with 11 elements
 * 0xd5 = fixext2 (needs type byte + 2 data bytes = 3 bytes after format byte)
 * Only 0x61,0x3a remain — 2 bytes, not enough.
 */
static void
test_issue363_fixext2_truncated(void)
{
	const unsigned char input[] = {0x9b, 0xd5, 0x61, 0x3a};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "fixext2 with insufficient data should fail, not overread");
}

/*
 * fixarray(1) + fixext1: needs 2 bytes (type + 1 data), provide only 1.
 * 0x91 = fixarray(1)
 * 0xd4 = fixext1, len from table = 1, parse_ignore sets len = 2
 * Only 1 byte (0xAA) remains.
 */
static void
test_fixext1_truncated(void)
{
	const unsigned char input[] = {0x91, 0xd4, 0xaa};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "fixext1 with insufficient data should fail");
}

/*
 * fixarray(1) + fixext4: needs 5 bytes (type + 4 data), provide only 3.
 * 0x91 = fixarray(1)
 * 0xd6 = fixext4, len from table = 4, parse_ignore sets len = 5
 */
static void
test_fixext4_truncated(void)
{
	const unsigned char input[] = {0x91, 0xd6, 0x01, 0x02, 0x03};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "fixext4 with insufficient data should fail");
}

/*
 * fixarray(1) + fixext8: needs 9 bytes (type + 8 data), provide only 4.
 * 0x91 = fixarray(1)
 * 0xd7 = fixext8, len from table = 8, parse_ignore sets len = 9
 */
static void
test_fixext8_truncated(void)
{
	const unsigned char input[] = {0x91, 0xd7, 0x01, 0x02, 0x03, 0x04};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "fixext8 with insufficient data should fail");
}

/*
 * fixarray(1) + fixext16: needs 17 bytes (type + 16 data), provide only 4.
 * 0x91 = fixarray(1)
 * 0xd8 = fixext16, len from table = 16, parse_ignore sets len = 17
 */
static void
test_fixext16_truncated(void)
{
	const unsigned char input[] = {0x91, 0xd8, 0x01, 0x02, 0x03, 0x04};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "fixext16 with insufficient data should fail");
}

/*
 * fixarray(1) + ext8 with length byte claiming 10 data bytes, but only 2 remain.
 * 0x91 = fixarray(1)
 * 0xc7 = ext8 (1-byte length prefix)
 * 0x0a = length = 10
 * parse_ignore does len = 10 + 1 = 11 (for the type byte)
 * Only 0xBB,0xCC remain (2 bytes).
 */
static void
test_ext8_truncated(void)
{
	const unsigned char input[] = {0x91, 0xc7, 0x0a, 0xbb, 0xcc};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "ext8 with insufficient data should fail");
}

/*
 * fixarray(1) + ext16 with 2-byte length claiming 256 data bytes, but only 1 remains.
 * 0x91 = fixarray(1)
 * 0xc8 = ext16 (2-byte length prefix, big-endian)
 * 0x01,0x00 = length = 256
 * parse_ignore does len = 256 + 1 = 257
 * Only 0xFF remains.
 */
static void
test_ext16_truncated(void)
{
	const unsigned char input[] = {0x91, 0xc8, 0x01, 0x00, 0xff};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "ext16 with insufficient data should fail");
}

/* ------------------------------------------------------------------ */
/* Other truncation / malformed input tests                            */
/* ------------------------------------------------------------------ */

/* Empty input — libucl accepts this (returns empty top-level object) */
static void
test_empty_input(void)
{
	const unsigned char input[] = {0};
	CHECK(parse_msgpack_ok(NULL, 0),
		  "empty input should succeed (empty top-level object)");
}

/* Single type byte with no payload for a type that requires one. */
static void
test_truncated_string(void)
{
	/* 0xd9 = str8 — needs at least 1 length byte, none available */
	const unsigned char input[] = {0x91, 0xd9};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "str8 with no length byte should fail");
}

/* Map with truncated key */
static void
test_truncated_map_key(void)
{
	/* 0x81 = fixmap(1) — expects 1 key-value pair
	 * 0xa5 = fixstr(5) — but only 2 bytes of string follow */
	const unsigned char input[] = {0x81, 0xa5, 'a', 'b'};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "map with truncated key should fail");
}

/* Truncated integer */
static void
test_truncated_int(void)
{
	/* 0x91 = fixarray(1)
	 * 0xce = uint32, needs 4 bytes, only 2 available */
	const unsigned char input[] = {0x91, 0xce, 0x00, 0x01};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "uint32 with insufficient data should fail");
}

/* Unknown format byte */
static void
test_unknown_format(void)
{
	/* 0xc1 is never-used / reserved in msgpack */
	const unsigned char input[] = {0x91, 0xc1};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "reserved format byte 0xc1 should fail");
}

/* ------------------------------------------------------------------ */
/* Correctness fixes that came out of the cbor work                     */
/* ------------------------------------------------------------------ */

/*
 * A uint64 above INT64_MAX used to wrap round to a negative number rather
 * than being refused
 */
static void
test_uint64_overflow(void)
{
	/* 0x91 = fixarray(1), 0xcf = uint64, value 2^64 - 1 */
	const unsigned char input[] = {0x91, 0xcf, 0xff, 0xff, 0xff, 0xff,
								   0xff, 0xff, 0xff, 0xff};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "uint64 that does not fit into int64 should fail");
}

/* INT64_MAX itself still has to go through */
static void
test_uint64_max_ok(void)
{
	const unsigned char input[] = {0x91, 0xcf, 0x7f, 0xff, 0xff, 0xff,
								   0xff, 0xff, 0xff, 0xff};
	CHECK(parse_msgpack_ok(input, sizeof(input)),
		  "uint64 holding INT64_MAX should succeed");
}

/*
 * A second document used to be parsed and then dropped, taking everything it
 * allocated with it
 */
static void
test_no_concatenation(void)
{
	/* 0x81 = fixmap(1) { "a": 1 } */
	const unsigned char doc[] = {0x81, 0xa1, 'a', 0x01};
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj;

	if (!ucl_parser_add_chunk_full(parser, doc, sizeof(doc), 0,
								   UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK)) {
		ucl_parser_free(parser);
		FAIL("%s", "first document should parse");
	}

	if (ucl_parser_add_chunk_full(parser, doc, sizeof(doc), 0,
								  UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK)) {
		ucl_parser_free(parser);
		FAIL("%s", "second document should be refused, not dropped");
	}

	obj = ucl_parser_get_object(parser);

	if (obj == NULL) {
		ucl_parser_free(parser);
		FAIL("%s", "the first document should survive the refusal");
	}

	ucl_object_unref(obj);
	ucl_parser_free(parser);
}

/*
 * Userdata used to emit obj->value, which holds an opaque pointer and a
 * length of zero, instead of what its emitter renders
 */
static void
test_userdata_emit(void)
{
	/* fixarray(1) holding fixstr(4) "null", the rendering with no emitter */
	const unsigned char expect[] = {0x91, 0xa4, 'n', 'u', 'l', 'l'};
	ucl_object_t *top = ucl_object_typed_new(UCL_ARRAY);
	unsigned char *out;
	size_t len = 0;

	ucl_array_append(top, ucl_object_new_userdata(NULL, NULL, NULL));
	out = ucl_object_emit_len(top, UCL_EMIT_MSGPACK, &len);

	if (out == NULL || len != sizeof(expect) ||
		memcmp(out, expect, sizeof(expect)) != 0) {
		free(out);
		ucl_object_unref(top);
		FAIL("%s", "userdata should emit what its emitter renders");
	}

	free(out);
	ucl_object_unref(top);
}

/*
 * A ucl chunk leaves its implicit top object on the parser stack, and that
 * object is also parser->top_obj. A failing msgpack chunk used to run
 * ucl_msgpack_release_tree over those frames, unref that object and leave
 * ucl_parser_free to unref it a second time. The mixed state must be refused
 * instead, without disturbing the ucl parser's own bookkeeping.
 */
static void
test_after_ucl_open_object(void)
{
	/* 0x92 0x01 = array(2) with a single element, truncated */
	const unsigned char bad[] = {0x92, 0x01};
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj;
	const ucl_object_t *elt;

	if (!ucl_parser_add_string(parser, "a = 1\n", 6)) {
		ucl_parser_free(parser);
		FAIL("%s", "the ucl chunk should parse");
	}

	if (ucl_parser_add_chunk_full(parser, bad, sizeof(bad),
								  0, UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK)) {
		ucl_parser_free(parser);
		FAIL("%s", "msgpack after an open ucl object should be refused");
	}

	/* The refusal must leave the ucl parser intact and usable */
	if (!ucl_parser_add_string(parser, "b = 2\n", 6)) {
		ucl_parser_free(parser);
		FAIL("%s", "the ucl parser should survive the refused msgpack chunk");
	}

	obj = ucl_parser_get_object(parser);

	if (obj == NULL) {
		ucl_parser_free(parser);
		FAIL("%s", "no object after the mixed chunks");
	}

	elt = ucl_object_lookup(obj, "b");

	if (elt == NULL || ucl_object_toint(elt) != 2) {
		ucl_object_unref(obj);
		ucl_parser_free(parser);
		FAIL("%s", "the chunk after the refusal was lost");
	}

	ucl_object_unref(obj);
	ucl_parser_free(parser);
}

/*
 * Merging across container kinds used to decode the incoming container as
 * the survivor's kind: map keys were dropped on the way into an array, and
 * array elements reached a map with no key at all. Same-kind merges must
 * keep working.
 */
static void
test_merge_container_type_mismatch(void)
{
	/* {"a": [7], "a": {"x":1}} */
	static const unsigned char map_into_array[] = {
		0x82,
		0xa1, 0x61, 0x91, 0x07,
		0xa1, 0x61, 0x81, 0xa1, 0x78, 0x01};
	/* {"a": {"x":1}, "a": [9,9]} */
	static const unsigned char array_into_map[] = {
		0x82,
		0xa1, 0x61, 0x81, 0xa1, 0x78, 0x01,
		0xa1, 0x61, 0x92, 0x09, 0x09};
	/* {"a": {"x":1}, "a": {"y":2}} merges to {"a":{"x":1,"y":2}} */
	static const unsigned char map_into_map[] = {
		0x82,
		0xa1, 0x61, 0x81, 0xa1, 0x78, 0x01,
		0xa1, 0x61, 0x81, 0xa1, 0x79, 0x02};
	/* {"a": [7], "a": [8]} merges to {"a":[7,8]} */
	static const unsigned char array_into_array[] = {
		0x82,
		0xa1, 0x61, 0x91, 0x07,
		0xa1, 0x61, 0x91, 0x08};
	/*
	 * Empty containers at the very end of input: the finishing state used
	 * to ignore the type check's verdict and report success with the error
	 * string attached
	 */
	/* {"a": [7], "a": {}} */
	static const unsigned char empty_map_into_array[] = {
		0x82,
		0xa1, 0x61, 0x91, 0x07,
		0xa1, 0x61, 0x80};
	/* {"a": {"x":1}, "a": []} */
	static const unsigned char empty_array_into_map[] = {
		0x82,
		0xa1, 0x61, 0x81, 0xa1, 0x78, 0x01,
		0xa1, 0x61, 0x90};
	/* {"a": {"x":1}, "a": {}} merges to {"a":{"x":1}} */
	static const unsigned char empty_map_into_map[] = {
		0x82,
		0xa1, 0x61, 0x81, 0xa1, 0x78, 0x01,
		0xa1, 0x61, 0x80};
	static const struct {
		const char *what;
		const unsigned char *data;
		size_t len;
		bool must_fail;
	} cases[] = {
		{"map into array", map_into_array, sizeof(map_into_array), true},
		{"array into map", array_into_map, sizeof(array_into_map), true},
		{"map into map", map_into_map, sizeof(map_into_map), false},
		{"array into array", array_into_array, sizeof(array_into_array),
		 false},
		{"empty map into array", empty_map_into_array,
		 sizeof(empty_map_into_array), true},
		{"empty array into map", empty_array_into_map,
		 sizeof(empty_array_into_map), true},
		{"empty map into map", empty_map_into_map,
		 sizeof(empty_map_into_map), false}};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
		bool ok;

		ok = ucl_parser_add_chunk_full(parser, cases[i].data, cases[i].len,
									   0, UCL_DUPLICATE_MERGE,
									   UCL_PARSE_MSGPACK);

		if (ok != !cases[i].must_fail) {
			ucl_parser_free(parser);
			FAIL("merge %s: %s", cases[i].what,
				 ok ? "accepted a cross-kind merge" :
					  "rejected a same-kind merge");
		}

		ucl_parser_free(parser);
	}
}

/*
 * With UCL_DUPLICATE_MERGE, a scalar for a key that holds a container used to
 * be dropped. Only containers merge, so like any other duplicate of the same
 * priority the scalar has to join the container in an implicit array.
 */
static void
test_merge_scalar_over_container(void)
{
	/* {"a": [7], "a": 5} */
	static const unsigned char int_over_array[] = {
		0x82,
		0xa1, 0x61, 0x91, 0x07,
		0xa1, 0x61, 0x05};
	/* {"a": {"x":1}, "a": "s"} */
	static const unsigned char string_over_map[] = {
		0x82,
		0xa1, 0x61, 0x81, 0xa1, 0x78, 0x01,
		0xa1, 0x61, 0xa1, 0x73};
	/* {"a": [7], "a": nil} */
	static const unsigned char nil_over_array[] = {
		0x82,
		0xa1, 0x61, 0x91, 0x07,
		0xa1, 0x61, 0xc0};
	static const struct {
		const char *what;
		const unsigned char *data;
		size_t len;
		ucl_type_t container;
		ucl_type_t scalar;
	} cases[] = {
		{"int over array", int_over_array, sizeof(int_over_array),
		 UCL_ARRAY, UCL_INT},
		{"string over map", string_over_map, sizeof(string_over_map),
		 UCL_OBJECT, UCL_STRING},
		{"nil over array", nil_over_array, sizeof(nil_over_array),
		 UCL_ARRAY, UCL_NULL}};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
		ucl_object_t *obj = NULL;
		const ucl_object_t *elt, *first, *second;
		ucl_object_iter_t it = NULL;
		bool ok;

		if (ucl_parser_add_chunk_full(parser, cases[i].data, cases[i].len,
									  0, UCL_DUPLICATE_MERGE,
									  UCL_PARSE_MSGPACK)) {
			obj = ucl_parser_get_object(parser);
		}

		ucl_parser_free(parser);
		CHECK(obj != NULL, cases[i].what);

		/* Iterate the implicit array rather than the container's elements */
		elt = ucl_object_lookup(obj, "a");
		first = ucl_object_iterate(elt, &it, false);
		second = ucl_object_iterate(elt, &it, false);
		ok = first != NULL && ucl_object_type(first) == cases[i].container &&
			 first->len == 1 &&
			 second != NULL && ucl_object_type(second) == cases[i].scalar &&
			 ucl_object_iterate(elt, &it, false) == NULL;
		ucl_object_unref(obj);

		if (!ok) {
			FAIL("merge %s: the scalar did not join the container",
				 cases[i].what);
		}
	}
}

/*
 * Without implicit arrays the values of a repeated key are kept in an
 * explicit array. With UCL_DUPLICATE_MERGE a container has to be merged into
 * the first of them, as into the head of an implicit array, and not into that
 * array: a map could not be merged at all, and the elements of an array were
 * added to the values of the key.
 */
static void
test_merge_no_implicit_arrays(void)
{
	/* {"a": {"x":1}, "a": 5, "a": {"y":2}} */
	static const unsigned char map_int_map[] = {
		0x83,
		0xa1, 0x61, 0x81, 0xa1, 0x78, 0x01,
		0xa1, 0x61, 0x05,
		0xa1, 0x61, 0x81, 0xa1, 0x79, 0x02};
	/* {"a": [7], "a": 5, "a": [8]} */
	static const unsigned char array_int_array[] = {
		0x83,
		0xa1, 0x61, 0x91, 0x07,
		0xa1, 0x61, 0x05,
		0xa1, 0x61, 0x91, 0x08};
	static const struct {
		const char *what;
		const unsigned char *data;
		size_t len;
		const char *expect;
	} cases[] = {
		{"map, int, map", map_int_map, sizeof(map_int_map),
		 "{\"a\":[{\"x\":1,\"y\":2},5]}"},
		{"array, int, array", array_int_array, sizeof(array_int_array),
		 "{\"a\":[[7,8],5]}"}};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO |
												   UCL_PARSER_NO_IMPLICIT_ARRAYS);
		ucl_object_t *obj = NULL;
		unsigned char *json;
		bool ok;

		if (ucl_parser_add_chunk_full(parser, cases[i].data, cases[i].len,
									  0, UCL_DUPLICATE_MERGE,
									  UCL_PARSE_MSGPACK)) {
			obj = ucl_parser_get_object(parser);
		}

		ucl_parser_free(parser);
		CHECK(obj != NULL, cases[i].what);

		json = ucl_object_emit(obj, UCL_EMIT_JSON_COMPACT);
		ok = json != NULL &&
			 strcmp((const char *) json, cases[i].expect) == 0;
		free(json);
		ucl_object_unref(obj);

		if (!ok) {
			FAIL("merge %s: expected %s", cases[i].what, cases[i].expect);
		}
	}
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	/* Issue #363 regression tests */
	test_issue363_fixext2_truncated();
	test_fixext1_truncated();
	test_fixext4_truncated();
	test_fixext8_truncated();
	test_fixext16_truncated();
	test_ext8_truncated();
	test_ext16_truncated();

	/* General malformed input tests */
	test_empty_input();
	test_truncated_string();
	test_truncated_map_key();
	test_truncated_int();
	test_unknown_format();

	/* Correctness fixes */
	test_uint64_overflow();
	test_uint64_max_ok();
	test_no_concatenation();
	test_userdata_emit();
	test_after_ucl_open_object();
	test_merge_container_type_mismatch();
	test_merge_scalar_over_container();
	test_merge_no_implicit_arrays();

	if (failed) {
		fprintf(stderr, "%d test(s) FAILED\n", failed);
		return 1;
	}

	printf("All msgpack malformed-input tests passed\n");
	return 0;
}
