/* Copyright (c) 2026, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
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
 * Parser budgets (ucl_parser_set_limits), the include nesting limit and the
 * iterative tree destructor.
 * The destruction tests matter as much as the limit ones: they are what keeps
 * a deeply nested document from exhausting the stack on cleanup, long after
 * the parser has returned successfully.
 */

#include "ucl.h"
#include "ucl_internal.h"

/* The checks below call the code under test, so they must run in release builds */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static struct ucl_parser *
make_parser(const struct ucl_parser_limits *limits)
{
	struct ucl_parser *p = ucl_parser_new(0);

	assert(p != NULL);

	if (limits != NULL) {
		ucl_parser_set_limits(p, limits);
	}

	return p;
}

static bool
parse(struct ucl_parser *p, const char *input, size_t len)
{
	return ucl_parser_add_chunk(p, (const unsigned char *) input, len);
}

static bool
parse_msgpack(struct ucl_parser *p, const char *input, size_t len)
{
	return ucl_parser_add_chunk_full(p, (const unsigned char *) input, len,
									 ucl_parser_get_default_priority(p),
									 UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK);
}

/* `depth` nested arrays: [[[...]]] */
static char *
nested_arrays(unsigned depth, size_t *len)
{
	char *res = malloc(depth * 2 + 1);

	assert(res != NULL);
	memset(res, '[', depth);
	memset(res + depth, ']', depth);
	res[depth * 2] = '\0';
	*len = depth * 2;

	return res;
}

/* `depth` nested objects: {"a":{"a":...1...}} */
static char *
nested_objects(unsigned depth, size_t *len)
{
	size_t sz = (size_t) depth * 6 + 2;
	char *res = malloc(sz);
	size_t i, pos = 0;

	assert(res != NULL);

	for (i = 0; i < depth; i++) {
		memcpy(res + pos, "{\"a\":", 5);
		pos += 5;
	}

	res[pos++] = '1';

	for (i = 0; i < depth; i++) {
		res[pos++] = '}';
	}

	res[pos] = '\0';
	*len = pos;

	return res;
}

/* `depth` fixarrays each holding one element, innermost being int 1 */
static char *
msgpack_nested_arrays(unsigned depth, size_t *len)
{
	char *res = malloc(depth + 1);

	assert(res != NULL);
	memset(res, '\x91', depth);
	res[depth] = '\x01';
	*len = depth + 1;

	return res;
}

/* One array16 holding `n` ints */
static char *
msgpack_flat_array(unsigned n, size_t *len)
{
	char *res = malloc(n + 3);

	assert(res != NULL);
	res[0] = '\xdc';
	res[1] = (char) (n >> 8);
	res[2] = (char) (n & 0xff);
	memset(res + 3, '\x01', n);
	*len = n + 3;

	return res;
}

static void
test_depth(void)
{
	struct ucl_parser *p;
	struct ucl_parser_limits limits;
	ucl_object_t *top;
	char *input;
	size_t len;

	/* Nesting is bounded by default, for arrays and for objects alike */
	p = make_parser(NULL);
	input = nested_arrays(100000, &len);
	assert(parse(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ENESTED);
	free(input);
	ucl_parser_free(p);

	p = make_parser(NULL);
	input = nested_objects(100000, &len);
	assert(parse(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ENESTED);
	free(input);
	ucl_parser_free(p);

	/* Just under the default limit still parses - and still frees */
	p = make_parser(NULL);
	input = nested_arrays(UCL_MAX_NESTING - 1, &len);
	assert(parse(p, input, len) == true);
	top = ucl_parser_get_object(p);
	assert(top != NULL);
	ucl_object_unref(top);
	free(input);
	ucl_parser_free(p);

	/* A custom depth limit overrides the default, in both directions */
	memset(&limits, 0, sizeof(limits));
	limits.max_depth = 4;
	p = make_parser(&limits);
	input = nested_arrays(5, &len);
	assert(parse(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ENESTED);
	free(input);
	ucl_parser_free(p);

	p = make_parser(&limits);
	input = nested_arrays(4, &len);
	assert(parse(p, input, len) == true);
	free(input);
	ucl_parser_free(p);

	/*
	 * Depth is measured per container rather than over the whole document:
	 * a long run of shallow siblings must not accumulate.
	 */
	memset(&limits, 0, sizeof(limits));
	limits.max_depth = 3;
	p = make_parser(&limits);
	assert(parse(p, "{\"a\": [1], \"b\": [2], \"c\": [3], \"d\": [4]}",
				 strlen("{\"a\": [1], \"b\": [2], \"c\": [3], \"d\": [4]}")) == true);
	ucl_parser_free(p);
}

/*
 * The { of a chunk that goes on in an open object has a frame of its own, but
 * no container of its own: it neither counts towards max_depth nor gives any
 * depth back when it is closed.
 */
static void
test_braced_chunk_depth(void)
{
	static const char fits[] = "{ c { d = 1 } }", deep[] = "{ c { d {} } }";
	struct ucl_parser_limits limits;
	struct ucl_parser *p;

	memset(&limits, 0, sizeof(limits));
	limits.max_depth = 2;

	/* The root takes one level, which leaves one for c */
	p = make_parser(&limits);
	assert(parse(p, "a = 1;", strlen("a = 1;")) == true);
	assert(ucl_parser_insert_chunk(p, (const unsigned char *) fits,
								   sizeof(fits) - 1) == true);
	assert(p->cur_depth == 1);
	ucl_parser_free(p);

	p = make_parser(&limits);
	assert(parse(p, "a = 1;", strlen("a = 1;")) == true);
	assert(ucl_parser_insert_chunk(p, (const unsigned char *) deep,
								   sizeof(deep) - 1) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ENESTED);
	ucl_parser_free(p);
}

static void
test_nodes_and_alloc(void)
{
	struct ucl_parser *p;
	struct ucl_parser_limits limits;

	/* max_nodes bounds the element count */
	memset(&limits, 0, sizeof(limits));
	limits.max_nodes = 5;
	p = make_parser(&limits);
	assert(parse(p, "[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]",
				 strlen("[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]")) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);

	/* ... and leaves conforming input alone */
	memset(&limits, 0, sizeof(limits));
	limits.max_nodes = 64;
	p = make_parser(&limits);
	assert(parse(p, "[1, 2, 3]", strlen("[1, 2, 3]")) == true);
	ucl_parser_free(p);

	/* max_alloc bounds the memory a small input can claim */
	memset(&limits, 0, sizeof(limits));
	limits.max_alloc = 512;
	p = make_parser(&limits);
	{
		char buf[8192];
		size_t pos = 0, i;

		buf[pos++] = '[';

		for (i = 0; i < 1000; i++) {
			buf[pos++] = '1';
			buf[pos++] = ',';
		}

		buf[pos - 1] = ']';
		assert(parse(p, buf, pos) == false);
		assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	}
	ucl_parser_free(p);

	/*
	 * Duplicate keys build an implicit-array wrapper under
	 * NO_IMPLICIT_ARRAYS; that wrapper is an element too and has to be
	 * charged, or a run of duplicates grows the tree for free.
	 */
	memset(&limits, 0, sizeof(limits));
	limits.max_nodes = 3;
	p = ucl_parser_new(UCL_PARSER_NO_IMPLICIT_ARRAYS);
	assert(p != NULL);
	ucl_parser_set_limits(p, &limits);
	assert(parse(p, "{a = 1; a = 2; a = 3;}",
				 strlen("{a = 1; a = 2; a = 3;}")) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);
}

static void
test_string_and_key_limits(void)
{
	struct ucl_parser *p;
	struct ucl_parser_limits limits;

	memset(&limits, 0, sizeof(limits));
	limits.max_key_length = 4;

	p = make_parser(&limits);
	assert(parse(p, "{\"abcde\": 1}", strlen("{\"abcde\": 1}")) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);

	/* A key exactly at the limit is fine */
	p = make_parser(&limits);
	assert(parse(p, "{\"abcd\": 1}", strlen("{\"abcd\": 1}")) == true);
	ucl_parser_free(p);

	memset(&limits, 0, sizeof(limits));
	limits.max_string_length = 4;

	p = make_parser(&limits);
	assert(parse(p, "{\"a\": \"abcde\"}", strlen("{\"a\": \"abcde\"}")) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);

	p = make_parser(&limits);
	assert(parse(p, "{\"a\": \"abcd\"}", strlen("{\"a\": \"abcd\"}")) == true);
	ucl_parser_free(p);

	/*
	 * Escape-heavy strings are charged for what they allocate, not for what
	 * they decode to: eight source bytes per decoded byte would otherwise
	 * slip past the budget.
	 */
	memset(&limits, 0, sizeof(limits));
	limits.max_alloc = 256;
	p = make_parser(&limits);
	{
		char buf[8192];
		size_t pos = 0, i;

		memcpy(buf, "{\"a\": \"", 7);
		pos = 7;

		for (i = 0; i < 500; i++) {
			memcpy(buf + pos, "\\u0041", 6);
			pos += 6;
		}

		memcpy(buf + pos, "\"}", 2);
		pos += 2;

		assert(parse(p, buf, pos) == false);
		assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	}
	ucl_parser_free(p);
}

static void
test_limits_accessors(void)
{
	struct ucl_parser *p;
	struct ucl_parser_limits set, got;

	/* Defaults: depth only */
	p = make_parser(NULL);
	memset(&got, 0xff, sizeof(got));
	ucl_parser_get_limits(p, &got);
	assert(got.max_depth == UCL_MAX_NESTING);
	assert(got.max_nodes == 0);
	assert(got.max_alloc == 0);
	assert(got.max_key_length == 0);
	assert(got.max_string_length == 0);

	/* Round trip */
	set.max_depth = 11;
	set.max_nodes = 22;
	set.max_alloc = 33;
	set.max_key_length = 44;
	set.max_string_length = 55;
	ucl_parser_set_limits(p, &set);
	memset(&got, 0, sizeof(got));
	ucl_parser_get_limits(p, &got);
	assert(memcmp(&set, &got, sizeof(set)) == 0);

	/* NULL restores the defaults */
	ucl_parser_set_limits(p, NULL);
	ucl_parser_get_limits(p, &got);
	assert(got.max_depth == UCL_MAX_NESTING);
	assert(got.max_nodes == 0);
	ucl_parser_free(p);

	/* Zero means unlimited, including for the depth */
	memset(&set, 0, sizeof(set));
	p = make_parser(&set);
	{
		size_t len;
		char *input = nested_arrays(5000, &len);

		assert(parse(p, input, len) == true);
		free(input);
	}
	ucl_parser_free(p);

	/* NULL parser must not crash either accessor */
	ucl_parser_set_limits(NULL, &set);
	ucl_parser_get_limits(NULL, &got);
}

static void
test_msgpack_limits(void)
{
	struct ucl_parser *p;
	struct ucl_parser_limits limits;
	char *input;
	size_t len;

	/* Nesting is bounded by default - 100k levels in 100k bytes */
	p = make_parser(NULL);
	input = msgpack_nested_arrays(100000, &len);
	assert(parse_msgpack(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ENESTED);
	free(input);
	ucl_parser_free(p);

	/* A custom depth limit applies */
	memset(&limits, 0, sizeof(limits));
	limits.max_depth = 8;
	p = make_parser(&limits);
	input = msgpack_nested_arrays(9, &len);
	assert(parse_msgpack(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ENESTED);
	free(input);
	ucl_parser_free(p);

	/* Within the limit it parses, and the tree frees */
	p = make_parser(&limits);
	input = msgpack_nested_arrays(8, &len);
	assert(parse_msgpack(p, input, len) == true);
	free(input);
	ucl_parser_free(p);

	/* max_nodes */
	memset(&limits, 0, sizeof(limits));
	limits.max_nodes = 16;
	p = make_parser(&limits);
	input = msgpack_flat_array(1000, &len);
	assert(parse_msgpack(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	free(input);
	ucl_parser_free(p);

	/* max_alloc */
	memset(&limits, 0, sizeof(limits));
	limits.max_alloc = 512;
	p = make_parser(&limits);
	input = msgpack_flat_array(1000, &len);
	assert(parse_msgpack(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	free(input);
	ucl_parser_free(p);

	/* max_string_length: a fixarray holding one 5-byte fixstr */
	memset(&limits, 0, sizeof(limits));
	limits.max_string_length = 4;
	p = make_parser(&limits);
	assert(parse_msgpack(p, "\x91\xa5" "abcde", 7) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);

	p = make_parser(&limits);
	assert(parse_msgpack(p, "\x91\xa4" "abcd", 6) == true);
	ucl_parser_free(p);

	/* max_key_length: fixmap of one 5-byte key */
	memset(&limits, 0, sizeof(limits));
	limits.max_key_length = 4;
	p = make_parser(&limits);
	assert(parse_msgpack(p, "\x81\xa5" "abcde" "\x01", 8) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);

	/* Conforming input is unaffected */
	memset(&limits, 0, sizeof(limits));
	limits.max_depth = 16;
	limits.max_nodes = 1024;
	limits.max_alloc = 1024 * 1024;
	limits.max_key_length = 64;
	limits.max_string_length = 1024;
	p = make_parser(&limits);
	assert(parse_msgpack(p, "\x81\xa1" "a" "\x93\x01\x02\x03", 7) == true);
	{
		ucl_object_t *top = ucl_parser_get_object(p);
		const ucl_object_t *arr;

		assert(top != NULL);
		arr = ucl_object_lookup(top, "a");
		assert(arr != NULL);
		assert(ucl_object_type(arr) == UCL_ARRAY);
		assert(ucl_array_size(arr) == 3);
		ucl_object_unref(top);
	}
	ucl_parser_free(p);
}

/*
 * Without UCL_PARSER_ZEROCOPY the parser owns a copy of every string, so the
 * objects have to outlive the buffer they were parsed from. Binary strings are
 * not NUL terminated and take their own copying path, which used to leave the
 * value pointing into that buffer.
 */
static ucl_object_t *
parse_msgpack_from_heap(const char *doc, size_t len)
{
	struct ucl_parser *p = make_parser(NULL);
	unsigned char *buf = malloc(len);
	ucl_object_t *top;
	bool ok;

	assert(buf != NULL);
	memcpy(buf, doc, len);

	ok = ucl_parser_add_chunk_full(p, buf, len,
								   ucl_parser_get_default_priority(p),
								   UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK);
	top = ok ? ucl_parser_get_object(p) : NULL;

	/* Everything the value may legitimately point at is now gone */
	ucl_parser_free(p);
	free(buf);

	return top;
}

/* fixmap{"b": bin8 holding `payload`} */
static char *
msgpack_binary(const char *payload, size_t payload_len, size_t *len)
{
	char *res = malloc(payload_len + 5);

	assert(res != NULL);
	res[0] = '\x81';
	res[1] = '\xa1';
	res[2] = 'b';
	res[3] = '\xc4';
	res[4] = (char) payload_len;
	memcpy(res + 5, payload, payload_len);
	*len = payload_len + 5;

	return res;
}

static void
test_binary_string_ownership(void)
{
	static const char payload[] = "\x01\x02\x03\x04";
	ucl_object_t *top;
	const ucl_object_t *bin;
	const char *val;
	size_t doc_len, len;
	char *doc;

	doc = msgpack_binary(payload, 4, &doc_len);
	top = parse_msgpack_from_heap(doc, doc_len);
	free(doc);
	assert(top != NULL);

	bin = ucl_object_lookup(top, "b");
	assert(bin != NULL);
	assert((bin->flags & UCL_OBJECT_BINARY) != 0);

	len = 0;
	val = ucl_object_tolstring(bin, &len);
	assert(val != NULL);
	assert(len == 4);
	assert(memcmp(val, payload, 4) == 0);

	ucl_object_unref(top);

	/* An empty binary string owns nothing and must not reference the input */
	doc = msgpack_binary("", 0, &doc_len);
	top = parse_msgpack_from_heap(doc, doc_len);
	free(doc);
	assert(top != NULL);

	bin = ucl_object_lookup(top, "b");
	assert(bin != NULL);

	len = 1;
	val = ucl_object_tolstring(bin, &len);
	assert(val != NULL);
	assert(len == 0);

	ucl_object_unref(top);
}

/*
 * A zero length value carries no payload, so reading its type consumes the
 * last byte of the document and the state machine finishes the value after its
 * loop rather than inside it. That tail used to insert without a key, which an
 * array does not mind and a map cannot accept.
 */
static void
test_map_ending_in_empty_value(void)
{
	ucl_object_t *top;
	const ucl_object_t *b;
	char *doc;
	size_t doc_len;

	/* fixmap{"b": fixstr of 0} */
	top = parse_msgpack_from_heap("\x81\xa1" "b" "\xa0", 4);
	assert(top != NULL);
	b = ucl_object_lookup(top, "b");
	assert(b != NULL);
	assert(ucl_object_type(b) == UCL_STRING);
	assert(b->len == 0);
	ucl_object_unref(top);

	/* fixmap{"b": bin8 of 0} */
	doc = msgpack_binary("", 0, &doc_len);
	top = parse_msgpack_from_heap(doc, doc_len);
	free(doc);
	assert(top != NULL);
	b = ucl_object_lookup(top, "b");
	assert(b != NULL);
	assert(b->len == 0);
	ucl_object_unref(top);

	/* fixmap{"b": [""]} - the array must not inherit the map's key */
	top = parse_msgpack_from_heap("\x81\xa1" "b" "\x91\xa0", 5);
	assert(top != NULL);
	b = ucl_object_lookup(top, "b");
	assert(b != NULL);
	assert(ucl_object_type(b) == UCL_ARRAY);
	assert(ucl_array_size(b) == 1);
	ucl_object_unref(top);
}

/*
 * The destructor walks an explicit worklist, so a tree far deeper than the
 * C stack could survive must still be released without recursing.
 */
static void
test_deep_destruction(void)
{
	ucl_object_t *top, *cur;
	unsigned i;

	top = ucl_object_typed_new(UCL_ARRAY);
	assert(top != NULL);
	cur = top;

	for (i = 0; i < 200000; i++) {
		ucl_object_t *next = ucl_object_typed_new(UCL_ARRAY);

		assert(next != NULL);
		assert(ucl_array_append(cur, next));
		cur = next;
	}

	ucl_object_unref(top);
}

/* A subtree that somebody else still references must outlive its container */
static void
test_shared_subtree(void)
{
	struct ucl_parser *p = make_parser(NULL);
	ucl_object_t *top, *kept;
	const ucl_object_t *inner, *arr;

	assert(parse(p, "{\"a\": {\"b\": [1, 2, 3]}}",
				 strlen("{\"a\": {\"b\": [1, 2, 3]}}")) == true);

	top = ucl_parser_get_object(p);
	assert(top != NULL);

	inner = ucl_object_lookup(top, "a");
	assert(inner != NULL);
	kept = ucl_object_ref(inner);

	/*
	 * The tree has to actually die for this to test anything:
	 * ucl_parser_get_object() hands out a second reference, so releasing
	 * `top` alone leaves the parser owning everything.
	 */
	ucl_object_unref(top);
	ucl_parser_free(p);

	arr = ucl_object_lookup(kept, "b");
	assert(arr != NULL);
	assert(ucl_object_type(arr) == UCL_ARRAY);
	assert(ucl_array_size(arr) == 3);
	assert(ucl_object_toint(ucl_array_find_index(arr, 0)) == 1);
	assert(ucl_object_toint(ucl_array_find_index(arr, 2)) == 3);

	ucl_object_unref(kept);
}

/* Implicit array chains hang off `next`, which the worklist reuses */
static void
test_implicit_array_chain(void)
{
	struct ucl_parser *p = make_parser(NULL);
	ucl_object_t *top;
	const ucl_object_t *elt;

	assert(parse(p, "a = 1; a = 2; a = 3; a = 4;",
				 strlen("a = 1; a = 2; a = 3; a = 4;")) == true);

	top = ucl_parser_get_object(p);
	assert(top != NULL);

	elt = ucl_object_lookup(top, "a");
	assert(elt != NULL);
	assert(ucl_object_toint(elt) == 1);
	assert(elt->next != NULL);

	ucl_object_unref(top);
	ucl_parser_free(p);
}

#ifndef _WIN32
/*
 * The include nesting limit counts the includes in progress, so each include
 * gives its level back once its file is parsed. A URL include did not, and a
 * document with more URL includes in a row than that limit was rejected.
 * The URL half only runs when the library has been built with URL support.
 */
static void
test_sequential_includes(void)
{
	const char *dir = getenv("TEST_OUT_DIR");
	char path[PATH_MAX], real[PATH_MAX], line[PATH_MAX + 64];
	char *doc;
	size_t doc_len, doc_size;
	struct ucl_parser *p;
	ucl_object_t *top;
	FILE *out;
	unsigned i;
	int url;

	snprintf(path, sizeof(path), "%s/limits_include.inc",
			 dir != NULL ? dir : ".");
	out = fopen(path, "w");
	assert(out != NULL);
	fputs("key = 1;\n", out);
	assert(fclose(out) == 0);
	assert(realpath(path, real) != NULL);

	doc_size = (UCL_MAX_RECURSION * 2) * sizeof(line);
	doc = malloc(doc_size);
	assert(doc != NULL);

	for (url = 0; url < 2; url++) {
		doc_len = 0;

		for (i = 0; i < UCL_MAX_RECURSION * 2; i++) {
			int n = snprintf(line, sizeof(line), "%s\"%s%s\"\n",
							 url ? ".include(url=true) " : ".include ",
							 url ? "file://" : "", real);

			assert(n > 0 && (size_t) n < sizeof(line));
			memcpy(doc + doc_len, line, n);
			doc_len += n;
		}

		p = make_parser(NULL);

		if (!parse(p, doc, doc_len)) {
			const char *err = ucl_parser_get_error(p);

			if (url && err != NULL &&
				strstr(err, "URL support is disabled") != NULL) {
				printf("URL includes in a row skipped: no URL support\n");
				ucl_parser_free(p);
				continue;
			}

			fprintf(stderr, "%s includes in a row failed: %s\n",
					url ? "URL" : "file", err != NULL ? err : "no error");
			abort();
		}

		assert(p->recursion == 1);
		top = ucl_parser_get_object(p);
		assert(top != NULL);
		assert(ucl_object_toint(ucl_object_lookup(top, "key")) == 1);
		ucl_object_unref(top);
		ucl_parser_free(p);
	}

	free(doc);
	remove(path);
}

/*
 * An inserted chunk gives its level back even when it fails. An include that
 * failed inside it leaves its own chunk on top of the stack, so taking only
 * the top chunk off kept the inserted one: it pointed to the caller's data,
 * and its level was never given back.
 */
static void
test_failed_insert_include(void)
{
	const char *dir = getenv("TEST_OUT_DIR");
	char path[PATH_MAX], doc[PATH_MAX + 32];
	struct ucl_parser *p;
	struct ucl_chunk *base;
	FILE *out;
	int n;

	snprintf(path, sizeof(path), "%s/limits_bad_include.inc",
			 dir != NULL ? dir : ".");
	out = fopen(path, "w");
	assert(out != NULL);
	fputs("a = {{;\n", out);
	assert(fclose(out) == 0);
	n = snprintf(doc, sizeof(doc), ".include \"%s\"\n", path);
	assert(n > 0 && (size_t) n < sizeof(doc));

	p = make_parser(NULL);
	assert(parse(p, "a = 1;", strlen("a = 1;")) == true);
	base = p->chunks;
	assert(base != NULL && p->recursion == 1);
	assert(ucl_parser_insert_chunk(p, (const unsigned char *) doc, n) == false);
	assert(p->chunks == base && base->next == NULL);
	assert(p->recursion == 1);
	ucl_parser_free(p);
	remove(path);
}

static void
write_include(char *path, size_t len, const char *name, const char *content)
{
	const char *dir = getenv("TEST_OUT_DIR");
	FILE *out;

	snprintf(path, len, "%s/%s", dir != NULL ? dir : ".", name);
	out = fopen(path, "w");
	assert(out != NULL);
	fputs(content, out);
	assert(fclose(out) == 0);
}

/* Parse doc, which has to fail with err, and check the chunks are released */
static void
check_failed_include_parse(struct ucl_parser *p, const char *doc,
						   const char *err)
{
	const char *res_err;

	assert(parse(p, doc, strlen(doc)) == false);
	res_err = ucl_parser_get_error(p);
	if (res_err == NULL || strstr(res_err, err) == NULL) {
		fprintf(stderr, "'%s': expected '%s', got '%s'\n", doc, err,
				res_err != NULL ? res_err : "no error");
		abort();
	}
	assert(p->chunks != NULL && p->chunks->next == NULL);
	assert(p->recursion == 1);
}

/*
 * A file that fails to parse leaves its chunk on top of the stack, pointing to
 * the buffer that is unmapped once the include returns. A .try_include glob
 * went on with its other files and the parse: the including file then read
 * the buffer of the failed one, and an include that succeeded after it took
 * that chunk off instead of its own. The same happened when the search for an
 * include went on to the next path. A file that fails to parse now ends the
 * glob or the search with its error, as for .try_include of that file alone.
 * Without that, files that include each other through a glob would go on at
 * the nesting limit, each level trying all of them again.
 */
static void
test_failed_glob_include(void)
{
	const char *dir = getenv("TEST_OUT_DIR");
	char path[4][PATH_MAX], doc[PATH_MAX + 64], content[PATH_MAX + 64];
	struct ucl_parser *p;
	ucl_object_t *paths;
	unsigned i;
	int n;

	/* The first file fails in the middle, the second one parses */
	n = snprintf(content, sizeof(content),
				 ".include \"%s/limits_glob_missing.inc\"\nx = 1\n",
				 dir != NULL ? dir : ".");
	assert(n > 0 && (size_t) n < sizeof(content));
	write_include(path[0], sizeof(path[0]), "limits_glob_1.inc", content);
	write_include(path[1], sizeof(path[1]), "limits_glob_2.inc", "b = 2\n");
	n = snprintf(doc, sizeof(doc),
				 ".try_include(glob=true) \"%s/limits_glob_*.inc\"\nc = 3\n",
				 dir != NULL ? dir : ".");
	assert(n > 0 && (size_t) n < sizeof(doc));
	p = make_parser(NULL);
	check_failed_include_parse(p, doc, "limits_glob_missing.inc");
	ucl_parser_free(p);
	remove(path[0]);
	remove(path[1]);

	/* The search ends in the first path, whose file fails */
	snprintf(path[2], sizeof(path[2]), "%s/limits_path1",
			 dir != NULL ? dir : ".");
	snprintf(path[3], sizeof(path[3]), "%s/limits_path2",
			 dir != NULL ? dir : ".");
	assert(mkdir(path[2], 0755) == 0 || errno == EEXIST);
	assert(mkdir(path[3], 0755) == 0 || errno == EEXIST);
	write_include(path[0], sizeof(path[0]), "limits_path1/limits.inc",
				  ".include \"limits_missing.inc\"\nx = 1\n");
	write_include(path[1], sizeof(path[1]), "limits_path2/limits.inc",
				  "b = 2\n");
	paths = ucl_object_typed_new(UCL_ARRAY);
	assert(paths != NULL);
	ucl_array_append(paths, ucl_object_fromstring(path[2]));
	ucl_array_append(paths, ucl_object_fromstring(path[3]));
	p = make_parser(NULL);
	assert(ucl_set_include_path(p, paths));
	ucl_object_unref(paths);
	check_failed_include_parse(p, ".include \"limits.inc\"\nc = 3\n",
							   "limits_missing.inc");
	ucl_parser_free(p);
	remove(path[0]);
	remove(path[1]);
	rmdir(path[2]);
	rmdir(path[3]);

	/* Files that include each other */
	n = snprintf(content, sizeof(content),
				 ".try_include(glob=true) \"%s/limits_cycle_*.inc\"\n",
				 dir != NULL ? dir : ".");
	assert(n > 0 && (size_t) n < sizeof(content));
	for (i = 0; i < 3; i++) {
		char name[32];

		snprintf(name, sizeof(name), "limits_cycle_%u.inc", i);
		write_include(path[i], sizeof(path[i]), name, content);
	}
	p = make_parser(NULL);
	check_failed_include_parse(p, content, "maximum include nesting limit");
	ucl_parser_free(p);
	for (i = 0; i < 3; i++) {
		remove(path[i]);
	}
}

/*
 * .try_include of a file that cannot be included, missing or the including
 * file itself, goes on with the parse. It made the macro fail with no error,
 * so the rest of the document was dropped, and a glob member ending with it
 * failed to parse, which ends the glob.
 */
static void
test_skipped_try_include(void)
{
	const char *dir = getenv("TEST_OUT_DIR");
	char path[2][PATH_MAX], doc[PATH_MAX + 64], content[3 * PATH_MAX + 64];
	struct ucl_parser *p;
	ucl_object_t *top;
	int n;

	if (dir == NULL) {
		dir = ".";
	}

	snprintf(path[0], sizeof(path[0]), "%s/limits_try_1.inc", dir);
	n = snprintf(content, sizeof(content),
				 ".try_include \"%s/limits_absent.inc\"\na = 1\n"
				 ".try_include \"%s\"\n",
				 dir, path[0]);
	assert(n > 0 && (size_t) n < sizeof(content));
	write_include(path[0], sizeof(path[0]), "limits_try_1.inc", content);
	write_include(path[1], sizeof(path[1]), "limits_try_2.inc", "b = 2\n");

	n = snprintf(doc, sizeof(doc),
				 "x = 1\n.try_include(glob=true) \"%s/limits_try_*.inc\"\n",
				 dir);
	assert(n > 0 && (size_t) n < sizeof(doc));
	p = make_parser(NULL);
	assert(parse(p, doc, n) == true);
	assert(ucl_parser_get_error(p) == NULL);
	assert(p->chunks != NULL && p->chunks->next == NULL);
	assert(p->recursion == 1);
	top = ucl_parser_get_object(p);
	assert(top != NULL);
	assert(ucl_object_toint(ucl_object_lookup(top, "x")) == 1);
	assert(ucl_object_toint(ucl_object_lookup(top, "a")) == 1);
	assert(ucl_object_toint(ucl_object_lookup(top, "b")) == 2);
	ucl_object_unref(top);
	ucl_parser_free(p);

	n = snprintf(doc, sizeof(doc),
				 ".try_include \"%s/limits_absent.inc\"\nz = 4\n", dir);
	assert(n > 0 && (size_t) n < sizeof(doc));
	p = make_parser(NULL);
	assert(parse(p, doc, n) == true);
	assert(ucl_parser_get_error(p) == NULL);
	top = ucl_parser_get_object(p);
	assert(top != NULL);
	assert(ucl_object_toint(ucl_object_lookup(top, "z")) == 4);
	ucl_object_unref(top);
	ucl_parser_free(p);

	remove(path[0]);
	remove(path[1]);
}

/*
 * A signed include of a file without a valid signature has to fail. A build
 * without openssl skipped the check and loaded the file as it was.
 */
static void
test_unsigned_include(void)
{
	static const char *const macros[] = {
		".include(sign=true)",
		".includes",
		".try_include(sign=true)",
		".include(sign=true,glob=true)",
	};
	char path[PATH_MAX], doc[PATH_MAX + 64];
	const char *err;
	struct ucl_parser *p;
	ucl_object_t *top;
	int n;

	write_include(path, sizeof(path), "limits_unsigned.inc", "b = 2\n");

	for (size_t i = 0; i < sizeof(macros) / sizeof(macros[0]); i++) {
		n = snprintf(doc, sizeof(doc), "a = 1\n%s \"%s\"\n", macros[i], path);
		assert(n > 0 && (size_t) n < sizeof(doc));
		p = make_parser(NULL);
		assert(parse(p, doc, n) == false);
		err = ucl_parser_get_error(p);
		if (err == NULL || (strstr(err, "built without signatures support") == NULL &&
							strstr(err, ".sig") == NULL)) {
			fprintf(stderr, "'%s': unexpected error '%s'\n", doc,
					err != NULL ? err : "(null)");
			abort();
		}
		top = ucl_parser_get_object(p);
		assert(top == NULL || ucl_object_lookup(top, "b") == NULL);
		ucl_object_unref(top);
		ucl_parser_free(p);
	}

	remove(path);
}

/*
 * A required URL include that cannot be fetched reported no error: the
 * fetch set its message only when the URL did not have to exist.
 */
static void
test_missing_url_include(void)
{
	const char *dir = getenv("TEST_OUT_DIR");
	char doc[PATH_MAX + 64], real[PATH_MAX];
	const char *err;
	struct ucl_parser *p;
	int n;

	assert(realpath(dir != NULL ? dir : ".", real) != NULL);
	n = snprintf(doc, sizeof(doc),
				 ".include(url=true) \"file://%s/limits_absent_url.inc\"\n",
				 real);
	assert(n > 0 && (size_t) n < sizeof(doc));
	p = make_parser(NULL);
	assert(parse(p, doc, n) == false);
	err = ucl_parser_get_error(p);
	if (err != NULL && strstr(err, "URL support is disabled") != NULL) {
		printf("missing URL include skipped: no URL support\n");
	}
	else {
		assert(err != NULL && strstr(err, "limits_absent_url.inc") != NULL);
	}
	ucl_parser_free(p);
}
#endif

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	test_depth();
	test_braced_chunk_depth();
	test_nodes_and_alloc();
	test_string_and_key_limits();
	test_limits_accessors();
	test_msgpack_limits();
	test_binary_string_ownership();
	test_map_ending_in_empty_value();
	test_deep_destruction();
	test_shared_subtree();
	test_implicit_array_chain();
#ifndef _WIN32
	test_sequential_includes();
	test_failed_insert_include();
	test_failed_glob_include();
	test_skipped_try_include();
	test_unsigned_include();
	test_missing_url_include();
#endif

	printf("All limits tests passed\n");

	return 0;
}
