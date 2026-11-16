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
 * Decoder tests for the cbor parser: the well formed half is built from the
 * examples in RFC 8949 appendix A (wrapped in a container, since ucl has
 * nowhere to keep a bare scalar), the malformed half checks that broken input
 * is rejected rather than crashing or being quietly accepted.
 */

#include "ucl.h"
#include <stdio.h>
#include <string.h>

static int failed = 0;

#define FAIL(fmt, ...)                                       \
	do {                                                     \
		fprintf(stderr, "FAIL: " fmt "\n", ##__VA_ARGS__);   \
		failed++;                                            \
	} while (0)

/*
 * Parse `data` as cbor and compare the compact json rendering of the result
 * against `expect`.
 */
static void
check_decodes(const char *what, const unsigned char *data, size_t len,
			  const char *expect)
{
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj;
	unsigned char *json;

	if (!ucl_parser_add_chunk_full(parser, data, len, 0,
								   UCL_DUPLICATE_APPEND, UCL_PARSE_CBOR)) {
		FAIL("%s: %s", what, ucl_parser_get_error(parser));
		ucl_parser_free(parser);

		return;
	}

	obj = ucl_parser_get_object(parser);

	if (obj == NULL) {
		FAIL("%s: no object produced", what);
		ucl_parser_free(parser);

		return;
	}

	json = ucl_object_emit(obj, UCL_EMIT_JSON_COMPACT);

	if (json == NULL) {
		FAIL("%s: cannot emit json", what);
	}
	else if (strcmp((const char *) json, expect) != 0) {
		FAIL("%s: got %s, expected %s", what, (const char *) json, expect);
	}

	free(json);
	ucl_object_unref(obj);
	ucl_parser_free(parser);
}

/* Parsing must fail, and must not crash or leak the partial tree */
static void
check_rejects(const char *what, const unsigned char *data, size_t len)
{
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);

	if (ucl_parser_add_chunk_full(parser, data, len, 0,
								  UCL_DUPLICATE_APPEND, UCL_PARSE_CBOR)) {
		FAIL("%s: accepted malformed input", what);
	}

	ucl_parser_free(parser);
}

/*
 * Parse `data` and compare the single element of the top level array against
 * `expect` bit for bit: going through json would round the value off.
 */
static void
check_double(const char *what, const unsigned char *data, size_t len,
			 double expect)
{
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj;
	const ucl_object_t *elt;
	double got;

	if (!ucl_parser_add_chunk_full(parser, data, len, 0,
								   UCL_DUPLICATE_APPEND, UCL_PARSE_CBOR)) {
		FAIL("%s: %s", what, ucl_parser_get_error(parser));
		ucl_parser_free(parser);

		return;
	}

	obj = ucl_parser_get_object(parser);
	elt = ucl_array_find_index(obj, 0);

	if (elt == NULL || ucl_object_type(elt) != UCL_FLOAT) {
		FAIL("%s: not a float", what);
	}
	else {
		got = ucl_object_todouble(elt);

		if (memcmp(&got, &expect, sizeof(double)) != 0) {
			FAIL("%s: got %.20g, expected %.20g", what, got, expect);
		}
	}

	ucl_object_unref(obj);
	ucl_parser_free(parser);
}

/*
 * Parse `data` and compare the single element of the top level array against
 * a byte string, which json cannot represent faithfully.
 */
static void
check_binary(const char *what, const unsigned char *data, size_t len,
			 const char *expect, size_t expectlen)
{
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj;
	const ucl_object_t *elt;
	const char *got;
	size_t gotlen = 0;

	if (!ucl_parser_add_chunk_full(parser, data, len, 0,
								   UCL_DUPLICATE_APPEND, UCL_PARSE_CBOR)) {
		FAIL("%s: %s", what, ucl_parser_get_error(parser));
		ucl_parser_free(parser);

		return;
	}

	obj = ucl_parser_get_object(parser);
	elt = ucl_array_find_index(obj, 0);

	if (elt == NULL || ucl_object_type(elt) != UCL_STRING) {
		FAIL("%s: not a string", what);
	}
	else if (!(elt->flags & UCL_OBJECT_BINARY)) {
		FAIL("%s: byte string did not come back as binary", what);
	}
	else {
		got = ucl_object_tolstring(elt, &gotlen);

		if (gotlen != expectlen || memcmp(got, expect, expectlen) != 0) {
			FAIL("%s: got %zu bytes, expected %zu", what, gotlen, expectlen);
		}
	}

	ucl_object_unref(obj);
	ucl_parser_free(parser);
}

#define DECODES(name, expect, ...)                                \
	do {                                                          \
		static const unsigned char in[] = {__VA_ARGS__};          \
		check_decodes(name, in, sizeof(in), expect);              \
	} while (0)

#define REJECTS(name, ...)                               \
	do {                                                 \
		static const unsigned char in[] = {__VA_ARGS__}; \
		check_rejects(name, in, sizeof(in));             \
	} while (0)

#define IS_DOUBLE(name, expect, ...)                     \
	do {                                                 \
		static const unsigned char in[] = {__VA_ARGS__}; \
		check_double(name, in, sizeof(in), expect);      \
	} while (0)

#define IS_BINARY(name, expect, ...)                                  \
	do {                                                              \
		static const unsigned char in[] = {__VA_ARGS__};              \
		check_binary(name, in, sizeof(in), expect, sizeof(expect) - 1); \
	} while (0)

static void
test_integers(void)
{
	DECODES("[0]", "[0]", 0x81, 0x00);
	DECODES("[1]", "[1]", 0x81, 0x01);
	DECODES("[10]", "[10]", 0x81, 0x0a);
	DECODES("[23]", "[23]", 0x81, 0x17);
	DECODES("[24]", "[24]", 0x81, 0x18, 0x18);
	DECODES("[25]", "[25]", 0x81, 0x18, 0x19);
	DECODES("[100]", "[100]", 0x81, 0x18, 0x64);
	DECODES("[1000]", "[1000]", 0x81, 0x19, 0x03, 0xe8);
	DECODES("[1000000]", "[1000000]", 0x81, 0x1a, 0x00, 0x0f, 0x42, 0x40);
	DECODES("[1000000000000]", "[1000000000000]",
			0x81, 0x1b, 0x00, 0x00, 0x00, 0xe8, 0xd4, 0xa5, 0x10, 0x00);
	DECODES("[-1]", "[-1]", 0x81, 0x20);
	DECODES("[-10]", "[-10]", 0x81, 0x29);
	DECODES("[-100]", "[-100]", 0x81, 0x38, 0x63);
	DECODES("[-1000]", "[-1000]", 0x81, 0x39, 0x03, 0xe7);

	/* The extremes of what int64 can hold */
	DECODES("[INT64_MAX]", "[9223372036854775807]",
			0x81, 0x1b, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
	DECODES("[INT64_MIN]", "[-9223372036854775808]",
			0x81, 0x3b, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);

	/* 18446744073709551615 and -18446744073709551616 do not fit */
	REJECTS("uint64 overflow",
			0x81, 0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
	REJECTS("nint64 overflow",
			0x81, 0x3b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
}

static void
test_floats(void)
{
	/* Half precision, including the subnormal and the largest finite value */
	IS_DOUBLE("[0.0 half]", 0.0, 0x81, 0xf9, 0x00, 0x00);
	IS_DOUBLE("[-0.0 half]", -0.0, 0x81, 0xf9, 0x80, 0x00);
	IS_DOUBLE("[1.0 half]", 1.0, 0x81, 0xf9, 0x3c, 0x00);
	IS_DOUBLE("[1.5 half]", 1.5, 0x81, 0xf9, 0x3e, 0x00);
	IS_DOUBLE("[65504.0 half]", 65504.0, 0x81, 0xf9, 0x7b, 0xff);
	IS_DOUBLE("[5.960464477539063e-8 half]", 5.960464477539063e-8,
			  0x81, 0xf9, 0x00, 0x01);
	IS_DOUBLE("[0.00006103515625 half]", 0.00006103515625,
			  0x81, 0xf9, 0x04, 0x00);
	IS_DOUBLE("[-4.0 half]", -4.0, 0x81, 0xf9, 0xc4, 0x00);

	/* Single and double precision */
	IS_DOUBLE("[100000.0 float]", 100000.0,
			  0x81, 0xfa, 0x47, 0xc3, 0x50, 0x00);
	IS_DOUBLE("[3.4028234663852886e+38 float]", 3.4028234663852886e+38,
			  0x81, 0xfa, 0x7f, 0x7f, 0xff, 0xff);
	IS_DOUBLE("[1.5 double]", 1.5,
			  0x81, 0xfb, 0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	IS_DOUBLE("[1.1 double]", 1.1,
			  0x81, 0xfb, 0x3f, 0xf1, 0x99, 0x99, 0x99, 0x99, 0x99, 0x9a);
	IS_DOUBLE("[-4.1 double]", -4.1,
			  0x81, 0xfb, 0xc0, 0x10, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66);
	IS_DOUBLE("[1.0e+300 double]", 1.0e+300,
			  0x81, 0xfb, 0x7e, 0x37, 0xe4, 0x3c, 0x88, 0x00, 0x75, 0x9c);
}

static void
test_simple(void)
{
	DECODES("[false,true,null]", "[false,true,null]", 0x83, 0xf4, 0xf5, 0xf6);
	/* undefined has no ucl counterpart and becomes null */
	DECODES("[undefined]", "[null]", 0x81, 0xf7);
	/* simple(255), an unassigned value, likewise */
	DECODES("[simple(255)]", "[null]", 0x81, 0xf8, 0xff);

	/* simple(24) and below must use the one byte form */
	REJECTS("simple(16) in two bytes", 0x81, 0xf8, 0x10);
	/* Additional information 28, 29 and 30 are reserved */
	REJECTS("reserved ai 28", 0x81, 0x1c);
	REJECTS("reserved ai 30", 0x81, 0xfe);
}

static void
test_strings(void)
{
	DECODES("[\"\"]", "[\"\"]", 0x81, 0x60);
	DECODES("[\"a\"]", "[\"a\"]", 0x81, 0x61, 0x61);
	DECODES("[\"IETF\"]", "[\"IETF\"]", 0x81, 0x64, 0x49, 0x45, 0x54, 0x46);
	DECODES("[\"\\u00fc\"]", "[\"\xc3\xbc\"]", 0x81, 0x62, 0xc3, 0xbc);

	/* Byte strings come back as binary ucl strings */
	IS_BINARY("[h'']", "", 0x81, 0x40);
	IS_BINARY("[h'01020304']", "\x01\x02\x03\x04",
			  0x81, 0x44, 0x01, 0x02, 0x03, 0x04);
	/* A NUL in the middle survives, which is the point of the binary flag */
	IS_BINARY("[h'610062']", "a\x00" "b", 0x81, 0x43, 0x61, 0x00, 0x62);

	/* Indefinite length strings, concatenated on the way in */
	IS_BINARY("[(_ h'0102', h'030405')]", "\x01\x02\x03\x04\x05",
			  0x81, 0x5f, 0x42, 0x01, 0x02, 0x43, 0x03, 0x04, 0x05, 0xff);
	DECODES("[(_ \"strea\", \"ming\")]", "[\"streaming\"]",
			0x81, 0x7f, 0x65, 0x73, 0x74, 0x72, 0x65, 0x61,
			0x64, 0x6d, 0x69, 0x6e, 0x67, 0xff);
	DECODES("[(_ )]", "[\"\"]", 0x81, 0x7f, 0xff);

	/* A chunk of the wrong major type is not well formed */
	REJECTS("mixed chunk types", 0x81, 0x7f, 0x42, 0x01, 0x02, 0xff);
	/* Nor is a chunk that is itself of indefinite length */
	REJECTS("nested indefinite chunk", 0x81, 0x7f, 0x7f, 0xff, 0xff);
	REJECTS("unterminated indefinite string", 0x81, 0x7f, 0x61, 0x61);
	REJECTS("truncated string", 0x81, 0x64, 0x49, 0x45);
}

static void
test_containers(void)
{
	DECODES("[]", "[]", 0x80);
	DECODES("[1,2,3]", "[1,2,3]", 0x83, 0x01, 0x02, 0x03);
	DECODES("[1,[2,3],[4,5]]", "[1,[2,3],[4,5]]",
			0x83, 0x01, 0x82, 0x02, 0x03, 0x82, 0x04, 0x05);
	DECODES("{}", "{}", 0xa0);
	DECODES("{\"a\":1,\"b\":[2,3]}", "{\"a\":1,\"b\":[2,3]}",
			0xa2, 0x61, 0x61, 0x01, 0x61, 0x62, 0x82, 0x02, 0x03);
	DECODES("[\"a\",{\"b\":\"c\"}]", "[\"a\",{\"b\":\"c\"}]",
			0x82, 0x61, 0x61, 0xa1, 0x61, 0x62, 0x61, 0x63);

	/* 25 elements, so the count needs its own byte */
	DECODES("[1..25]",
			"[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25]",
			0x98, 0x19, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
			0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x18, 0x18, 0x19);
}

static void
test_indefinite_containers(void)
{
	DECODES("[_ ]", "[]", 0x9f, 0xff);
	DECODES("[_ 1, [2, 3], [_ 4, 5]]", "[1,[2,3],[4,5]]",
			0x9f, 0x01, 0x82, 0x02, 0x03, 0x9f, 0x04, 0x05, 0xff, 0xff);
	DECODES("[_ 1, [2, 3], [4, 5]]", "[1,[2,3],[4,5]]",
			0x9f, 0x01, 0x82, 0x02, 0x03, 0x82, 0x04, 0x05, 0xff);
	DECODES("{_ \"a\": 1, \"b\": [_ 2, 3]}", "{\"a\":1,\"b\":[2,3]}",
			0xbf, 0x61, 0x61, 0x01, 0x61, 0x62, 0x9f, 0x02, 0x03, 0xff, 0xff);
	DECODES("[\"a\", {_ \"b\": \"c\"}]", "[\"a\",{\"b\":\"c\"}]",
			0x82, 0x61, 0x61, 0xbf, 0x61, 0x62, 0x61, 0x63, 0xff);

	REJECTS("unterminated indefinite array", 0x9f, 0x01, 0x02);
	REJECTS("break with a dangling key", 0xbf, 0x61, 0x61, 0xff);
	REJECTS("break outside a container", 0x81, 0xff);
	REJECTS("break inside a definite array", 0x83, 0x01, 0xff, 0x03);
}

static void
test_tags(void)
{
	/* A tag is dropped and the item it wraps takes its place */
	DECODES("[0(\"2013-03-21T20:04:00Z\")]", "[\"2013-03-21T20:04:00Z\"]",
			0x81, 0xc0, 0x74, 0x32, 0x30, 0x31, 0x33, 0x2d, 0x30, 0x33,
			0x2d, 0x32, 0x31, 0x54, 0x32, 0x30, 0x3a, 0x30, 0x34, 0x3a,
			0x30, 0x30, 0x5a);
	DECODES("[1(1363896240)]", "[1363896240]",
			0x81, 0xc1, 0x1a, 0x51, 0x4b, 0x67, 0xb0);
	DECODES("[32(\"http://www.example.com\")]", "[\"http://www.example.com\"]",
			0x81, 0xd8, 0x20, 0x76, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f,
			0x2f, 0x77, 0x77, 0x77, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70,
			0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d);

	/* The self-described cbor tag in front of the document */
	DECODES("55799({\"a\":1})", "{\"a\":1}",
			0xd9, 0xd9, 0xf7, 0xa1, 0x61, 0x61, 0x01);

	REJECTS("tag with nothing after it", 0x81, 0xc0);
}

static void
test_keys(void)
{
	/* Byte string keys work the way msgpack's bin keys do */
	DECODES("{h'6162': 1}", "{\"ab\":1}",
			0xa1, 0x42, 0x61, 0x62, 0x01);
	/* Integer keys are rendered as their decimal spelling */
	DECODES("{1: \"a\", -1: \"b\"}", "{\"1\":\"a\",\"-1\":\"b\"}",
			0xa2, 0x01, 0x61, 0x61, 0x20, 0x61, 0x62);
	/* An indefinite length key is concatenated like any other string */
	DECODES("{(_ \"a\", \"b\"): 1}", "{\"ab\":1}",
			0xa1, 0x7f, 0x61, 0x61, 0x61, 0x62, 0xff, 0x01);

	REJECTS("array as a key", 0xa1, 0x80, 0x01);
	REJECTS("bool as a key", 0xa1, 0xf5, 0x01);
	REJECTS("empty key", 0xa1, 0x60, 0x01);
	REJECTS("key with no value", 0xa1, 0x61, 0x61);
}

static void
test_framing(void)
{
	REJECTS("bare integer at the top level", 0x01);
	REJECTS("bare string at the top level", 0x61, 0x61);
	REJECTS("trailing data", 0x80, 0x80);
	REJECTS("truncated array head", 0x9a, 0x00, 0x01);
	REJECTS("array claiming more elements than there are bytes",
			0x9a, 0x7f, 0xff, 0xff, 0xff);
	REJECTS("map claiming more pairs than there are bytes",
			0xba, 0x7f, 0xff, 0xff, 0xff);
	REJECTS("truncated nested array", 0x81, 0x82, 0x01);
}

/*
 * A cbor document carries its own root, so a second one has to be refused
 * rather than parsed and dropped
 */
static void
test_no_concatenation(void)
{
	static const unsigned char first[] = {0xa1, 0x61, 0x61, 0x01};
	static const unsigned char second[] = {0x82, 0x01, 0x02};
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj;

	if (!ucl_parser_add_chunk_full(parser, first, sizeof(first), 0,
								   UCL_DUPLICATE_APPEND, UCL_PARSE_CBOR)) {
		FAIL("concatenation: first document was rejected");
		ucl_parser_free(parser);

		return;
	}

	if (ucl_parser_add_chunk_full(parser, second, sizeof(second), 0,
								  UCL_DUPLICATE_APPEND, UCL_PARSE_CBOR)) {
		FAIL("concatenation: second document was accepted and lost");
	}

	obj = ucl_parser_get_object(parser);

	if (obj == NULL) {
		FAIL("concatenation: the first document did not survive");
	}
	else {
		ucl_object_unref(obj);
	}

	ucl_parser_free(parser);
}

/* Userdata has to go out as whatever its emitter renders */
static void
test_userdata(void)
{
	ucl_object_t *top = ucl_object_typed_new(UCL_ARRAY);
	unsigned char *out;
	size_t outlen;

	ucl_array_append(top, ucl_object_new_userdata(NULL, NULL, NULL));
	out = ucl_object_emit_len(top, UCL_EMIT_CBOR, &outlen);

	/*
	 * With no emitter attached the object renders as the literal "null",
	 * which is four bytes of text rather than the empty string the raw
	 * value pointer used to produce
	 */
	if (out == NULL || outlen != 6 || out[0] != 0x81 || out[1] != 0x64 ||
		memcmp(out + 2, "null", 4) != 0) {
		FAIL("userdata: got %zu bytes, expected the rendered string",
			 out == NULL ? (size_t) 0 : outlen);
	}

	free(out);
	ucl_object_unref(top);
}

static void
test_roundtrip(void)
{
	static const unsigned char doc[] = {
		0xa4,
		0x61, 0x69, 0x18, 0x2a,                                     /* i: 42 */
		0x61, 0x66, 0xfb, 0x3f, 0xf1, 0x99, 0x99, 0x99, 0x99, 0x99, /* f */
		0x9a,
		0x61, 0x73, 0x63, 0x61, 0x62, 0x63, /* s: "abc" */
		0x61, 0x61, 0x83, 0xf4, 0xf5, 0xf6  /* a: [false,true,null] */
	};
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj;
	unsigned char *out;
	size_t outlen;

	if (!ucl_parser_add_chunk_full(parser, doc, sizeof(doc), 0,
								   UCL_DUPLICATE_APPEND, UCL_PARSE_CBOR)) {
		FAIL("roundtrip: %s", ucl_parser_get_error(parser));
		ucl_parser_free(parser);

		return;
	}

	obj = ucl_parser_get_object(parser);
	out = ucl_object_emit_len(obj, UCL_EMIT_CBOR, &outlen);

	if (out == NULL) {
		FAIL("roundtrip: cannot emit");
	}
	else if (outlen != sizeof(doc) || memcmp(out, doc, outlen) != 0) {
		FAIL("roundtrip: re-encoded document differs (%zu vs %zu bytes)",
			 outlen, sizeof(doc));
	}

	free(out);
	ucl_object_unref(obj);
	ucl_parser_free(parser);
}

static void
test_limits(void)
{
	struct ucl_parser *parser;
	struct ucl_parser_limits limits;
	/* [[[[[[[[[[0]]]]]]]]]] - ten levels of nesting */
	static const unsigned char deep[] = {
		0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x00};
	static const unsigned char doc[] = {0x81, 0x64, 0x61, 0x62, 0x63, 0x64};

	parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_parser_get_limits(parser, &limits);
	limits.max_depth = 4;
	ucl_parser_set_limits(parser, &limits);

	if (ucl_parser_add_chunk_full(parser, deep, sizeof(deep), 0,
								  UCL_DUPLICATE_APPEND, UCL_PARSE_CBOR)) {
		FAIL("limits: max_depth was not enforced");
	}

	ucl_parser_free(parser);

	parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_parser_get_limits(parser, &limits);
	limits.max_string_length = 3;
	ucl_parser_set_limits(parser, &limits);

	if (ucl_parser_add_chunk_full(parser, doc, sizeof(doc), 0,
								  UCL_DUPLICATE_APPEND, UCL_PARSE_CBOR)) {
		FAIL("limits: max_string_length was not enforced");
	}

	ucl_parser_free(parser);

	parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_parser_get_limits(parser, &limits);
	limits.max_nodes = 1;
	ucl_parser_set_limits(parser, &limits);

	if (ucl_parser_add_chunk_full(parser, doc, sizeof(doc), 0,
								  UCL_DUPLICATE_APPEND, UCL_PARSE_CBOR)) {
		FAIL("limits: max_nodes was not enforced");
	}

	ucl_parser_free(parser);
}

/*
 * With UCL_DUPLICATE_MERGE, a duplicate key holding a container merges the
 * fresh container into the existing one and releases the fresh object. The
 * parser used to keep inserting into the released one.
 */
static void
test_merge(void)
{
	/* {"a": {"x":1}, "a": {"y":2}} */
	static const unsigned char doc[] = {
		0xa2,
		0x61, 0x61, 0xa1, 0x61, 0x78, 0x01, /* "a": {"x": 1} */
		0x61, 0x61, 0xa1, 0x61, 0x79, 0x02  /* "a": {"y": 2} */
	};
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj;
	unsigned char *json;

	if (!ucl_parser_add_chunk_full(parser, doc, sizeof(doc), 0,
								   UCL_DUPLICATE_MERGE, UCL_PARSE_CBOR)) {
		FAIL("merge: %s", ucl_parser_get_error(parser));
		ucl_parser_free(parser);

		return;
	}

	obj = ucl_parser_get_object(parser);
	json = ucl_object_emit(obj, UCL_EMIT_JSON_COMPACT);

	if (json == NULL ||
		strcmp((const char *) json, "{\"a\":{\"x\":1,\"y\":2}}") != 0) {
		FAIL("merge: got %s, expected the containers united",
			 json ? (const char *) json : "(null)");
	}

	free(json);
	ucl_object_unref(obj);
	ucl_parser_free(parser);
}

/*
 * Merging across container kinds used to decode the incoming container as
 * the survivor's kind: map keys became array elements and array elements
 * became integer keyed map entries, silently for indefinite containers.
 */
static void
test_merge_container_type_mismatch(void)
{
	/* {"a": [7], "a": {"x":1}}, definite and indefinite */
	static const unsigned char map_into_array_def[] = {
		0xa2,
		0x61, 0x61, 0x81, 0x07,
		0x61, 0x61, 0xa1, 0x61, 0x78, 0x01};
	static const unsigned char map_into_array_indef[] = {
		0xa2,
		0x61, 0x61, 0x81, 0x07,
		0x61, 0x61, 0xbf, 0x61, 0x78, 0x01, 0xff};
	/* {"a": {"x":1}, "a": [9,9]}, definite and indefinite */
	static const unsigned char array_into_map_def[] = {
		0xa2,
		0x61, 0x61, 0xa1, 0x61, 0x78, 0x01,
		0x61, 0x61, 0x82, 0x09, 0x09};
	static const unsigned char array_into_map_indef[] = {
		0xa2,
		0x61, 0x61, 0xa1, 0x61, 0x78, 0x01,
		0x61, 0x61, 0x9f, 0x09, 0x09, 0xff};
	static const struct {
		const char *what;
		const unsigned char *data;
		size_t len;
	} cases[] = {
		{"map into array (definite)", map_into_array_def,
		 sizeof(map_into_array_def)},
		{"map into array (indefinite)", map_into_array_indef,
		 sizeof(map_into_array_indef)},
		{"array into map (definite)", array_into_map_def,
		 sizeof(array_into_map_def)},
		{"array into map (indefinite)", array_into_map_indef,
		 sizeof(array_into_map_indef)}};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);

		if (ucl_parser_add_chunk_full(parser, cases[i].data, cases[i].len, 0,
									  UCL_DUPLICATE_MERGE, UCL_PARSE_CBOR)) {
			FAIL("merge mismatch %s: accepted a cross-kind merge",
				 cases[i].what);
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
		0xa2,
		0x61, 0x61, 0x81, 0x07,
		0x61, 0x61, 0x05};
	/* {"a": {"x":1}, "a": "s"} */
	static const unsigned char string_over_map[] = {
		0xa2,
		0x61, 0x61, 0xa1, 0x61, 0x78, 0x01,
		0x61, 0x61, 0x61, 0x73};
	/* {"a": [7], "a": null} */
	static const unsigned char null_over_array[] = {
		0xa2,
		0x61, 0x61, 0x81, 0x07,
		0x61, 0x61, 0xf6};
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
		{"null over array", null_over_array, sizeof(null_over_array),
		 UCL_ARRAY, UCL_NULL}};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
		ucl_object_t *obj;
		const ucl_object_t *elt, *first, *second;
		ucl_object_iter_t it = NULL;

		if (!ucl_parser_add_chunk_full(parser, cases[i].data, cases[i].len,
									   0, UCL_DUPLICATE_MERGE,
									   UCL_PARSE_CBOR)) {
			FAIL("merge %s: %s", cases[i].what,
				 ucl_parser_get_error(parser));
			ucl_parser_free(parser);

			continue;
		}

		obj = ucl_parser_get_object(parser);
		ucl_parser_free(parser);

		/* Iterate the implicit array rather than the container's elements */
		elt = ucl_object_lookup(obj, "a");
		first = ucl_object_iterate(elt, &it, false);
		second = ucl_object_iterate(elt, &it, false);

		if (first == NULL || ucl_object_type(first) != cases[i].container ||
			first->len != 1 ||
			second == NULL || ucl_object_type(second) != cases[i].scalar ||
			ucl_object_iterate(elt, &it, false) != NULL) {
			FAIL("merge %s: the scalar did not join the container",
				 cases[i].what);
		}

		ucl_object_unref(obj);
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
		0xa3,
		0x61, 0x61, 0xa1, 0x61, 0x78, 0x01,
		0x61, 0x61, 0x05,
		0x61, 0x61, 0xa1, 0x61, 0x79, 0x02};
	/* {"a": [7], "a": 5, "a": [8]} */
	static const unsigned char array_int_array[] = {
		0xa3,
		0x61, 0x61, 0x81, 0x07,
		0x61, 0x61, 0x05,
		0x61, 0x61, 0x81, 0x08};
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
		ucl_object_t *obj;
		unsigned char *json;

		if (!ucl_parser_add_chunk_full(parser, cases[i].data, cases[i].len,
									   0, UCL_DUPLICATE_MERGE,
									   UCL_PARSE_CBOR)) {
			FAIL("merge %s: %s", cases[i].what,
				 ucl_parser_get_error(parser));
			ucl_parser_free(parser);

			continue;
		}

		obj = ucl_parser_get_object(parser);
		ucl_parser_free(parser);
		json = ucl_object_emit(obj, UCL_EMIT_JSON_COMPACT);

		if (json == NULL ||
			strcmp((const char *) json, cases[i].expect) != 0) {
			FAIL("merge %s: got %s, expected %s", cases[i].what,
				 json ? (const char *) json : "(null)", cases[i].expect);
		}

		free(json);
		ucl_object_unref(obj);
	}
}

/*
 * A ucl chunk leaves its implicit top object on the parser stack, and a cbor
 * document fed after it used to be parsed into that foreign container. The
 * mixed state must be refused instead, without disturbing the ucl parser.
 */
static void
test_after_ucl_open_object(void)
{
	static const unsigned char doc[] = {0x80}; /* [] */
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj;
	const ucl_object_t *elt;

	if (!ucl_parser_add_string(parser, "a = 1\n", 6)) {
		ucl_parser_free(parser);
		FAIL("%s", "the ucl chunk should parse");
	}

	if (ucl_parser_add_chunk_full(parser, doc, sizeof(doc), 0,
								  UCL_DUPLICATE_APPEND, UCL_PARSE_CBOR)) {
		ucl_parser_free(parser);
		FAIL("%s", "cbor after an open ucl object should be refused");
	}

	if (!ucl_parser_add_string(parser, "b = 2\n", 6)) {
		ucl_parser_free(parser);
		FAIL("%s", "the ucl parser should survive the refused cbor chunk");
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

int main(int argc, char **argv)
{
	test_integers();
	test_floats();
	test_simple();
	test_strings();
	test_containers();
	test_indefinite_containers();
	test_tags();
	test_keys();
	test_framing();
	test_no_concatenation();
	test_userdata();
	test_roundtrip();
	test_limits();
	test_merge();
	test_merge_container_type_mismatch();
	test_merge_scalar_over_container();
	test_merge_no_implicit_arrays();
	test_after_ucl_open_object();

	if (failed > 0) {
		fprintf(stderr, "%d cbor test(s) failed\n", failed);

		return 1;
	}

	printf("All cbor decoder tests passed\n");

	return 0;
}
