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
 * Regression test for the duplicate-key error message in
 * ucl_parser_process_object_element. With UCL_PARSER_ZEROCOPY the parsed keys
 * are unterminated pointers into the caller's input (documented in ucl.h), so
 * the UCL_DUPLICATE_ERROR branch must not format nobj->key with a plain "%s":
 * that runs off the end of the input buffer. We feed the parser from an
 * exact-size heap buffer (no trailing NUL) so AddressSanitizer catches the
 * over-read at the first byte past the input.
 */

#include "ucl.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
test_zerocopy_duplicate_error(void)
{
	/* Two entries with the same key trigger UCL_DUPLICATE_ERROR. */
	static const char doc[] = "a=1;a=2";
	const size_t len = sizeof(doc) - 1;

	/*
	 * Exact-size buffer: no room for a terminating NUL, so any read past
	 * the key relies on whatever follows it in the input and, here, past
	 * the allocation itself.
	 */
	unsigned char *buf = malloc(len);
	assert(buf != NULL);
	memcpy(buf, doc, len);

	struct ucl_parser *p = ucl_parser_new(UCL_PARSER_ZEROCOPY);
	assert(p != NULL);

	bool res = ucl_parser_add_chunk_full(p, buf, len,
										 ucl_parser_get_default_priority(p),
										 UCL_DUPLICATE_ERROR, UCL_PARSE_UCL);

	/* The duplicate key must be reported as an error, not silently merged. */
	assert(!res);
	const char *err = ucl_parser_get_error(p);
	assert(err != NULL);
	/* The message names exactly the key, not the rest of the input. */
	assert(strstr(err, "duplicate element for key 'a' found") != NULL);

	ucl_parser_free(p);
	free(buf);
}

/*
 * With UCL_DUPLICATE_ERROR, the element of a repeated key was handed to the
 * parser to release, yet the msgpack and cbor readers released it as well on
 * the failure, and ucl_parser_free() then freed it once more.
 */
static void
test_binary_duplicate_error(void)
{
	static const struct {
		const unsigned char *data;
		size_t len;
		enum ucl_parse_type type;
	} cases[] = {
#define DOCUMENT(data, type) {(const unsigned char *) data, sizeof(data) - 1, type}
		/* {"a": 1, "a": 2} */
		DOCUMENT("\x82\xa1\x61\x01\xa1\x61\x02", UCL_PARSE_MSGPACK),
		/* {"a": {"x": 1}, "a": {"y": 2}} */
		DOCUMENT("\x82\xa1\x61\x81\xa1\x78\x01\xa1\x61\x81\xa1\x79\x02",
				 UCL_PARSE_MSGPACK),
		DOCUMENT("\xa2\x61\x61\x01\x61\x61\x02", UCL_PARSE_CBOR),
		DOCUMENT("\xa2\x61\x61\xa1\x61\x78\x01\x61\x61\xa1\x61\x79\x02",
				 UCL_PARSE_CBOR),
#undef DOCUMENT
	};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct ucl_parser *p = ucl_parser_new(0);
		assert(p != NULL);

		assert(!ucl_parser_add_chunk_full(p, cases[i].data, cases[i].len, 0,
										  UCL_DUPLICATE_ERROR, cases[i].type));
		assert(ucl_parser_get_error_code(p) == UCL_EMERGE);
		assert(strstr(ucl_parser_get_error(p),
					  "duplicate element for key 'a' found") != NULL);

		ucl_parser_free(p);
	}
}

/*
 * Without implicit arrays the values of a repeated key are kept in an
 * explicit array. With UCL_DUPLICATE_MERGE a container has to be merged into
 * the first of them, as into the head of an implicit array, and not into that
 * array: an object could not be merged at all, and the elements of an array
 * were added to the values of the key.
 */
static void
test_merge_no_implicit_arrays(void)
{
	static const struct {
		const char *doc;
		const char *expect;
	} cases[] = {
		{"a {x = 1}; a = 5; a {y = 2};", "{\"a\":[{\"x\":1,\"y\":2},5]}"},
		{"a [7]; a = 5; a [8];", "{\"a\":[[7,8],5]}"},
		/* Nothing to merge into when the first value is a scalar */
		{"a = 5; a {x = 1}; a {y = 2};", "{\"a\":[5,{\"x\":1},{\"y\":2}]}"},
		{"a = 5; a = 6; a [8];", "{\"a\":[5,6,[8]]}"},
	};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct ucl_parser *p = ucl_parser_new(UCL_PARSER_NO_IMPLICIT_ARRAYS);
		assert(p != NULL);

		bool res = ucl_parser_add_chunk_full(p,
											 (const unsigned char *) cases[i].doc,
											 strlen(cases[i].doc), 0,
											 UCL_DUPLICATE_MERGE, UCL_PARSE_UCL);

		if (!res) {
			fprintf(stderr, "'%s': %s\n", cases[i].doc, ucl_parser_get_error(p));
		}
		assert(res);

		ucl_object_t *obj = ucl_parser_get_object(p);
		ucl_parser_free(p);
		unsigned char *json = ucl_object_emit(obj, UCL_EMIT_JSON_COMPACT);
		assert(json != NULL);

		if (strcmp((const char *) json, cases[i].expect) != 0) {
			fprintf(stderr, "'%s': got %s, expected %s\n", cases[i].doc,
					(const char *) json, cases[i].expect);
		}
		assert(strcmp((const char *) json, cases[i].expect) == 0);

		free(json);
		ucl_object_unref(obj);
	}
}

/*
 * Without implicit arrays the values of a repeated key are kept in an
 * explicit array, which was created with priority 0 whatever the priority of
 * the values: at any higher priority, the third value of a key replaced the
 * first two, and a value of a lower priority chunk replaced all of them.
 */
static void
test_no_implicit_arrays_priority(void)
{
	static const struct {
		const char *doc;
		unsigned priority;
		const char *next;
		unsigned next_priority;
		enum ucl_duplicate_strategy strat;
		const char *expect;
	} cases[] = {
		{"a = 1; a = 2; a = 3;", 2, NULL, 0, UCL_DUPLICATE_APPEND,
		 "{\"a\":[1,2,3]}"},
		{"a = 1; a = 2; a = 3;", 2, NULL, 0, UCL_DUPLICATE_MERGE,
		 "{\"a\":[1,2,3]}"},
		{"a {x = 1}; a = 2; a = 3;", 2, NULL, 0, UCL_DUPLICATE_MERGE,
		 "{\"a\":[{\"x\":1},2,3]}"},
		{"a = 1; a = 2;", 2, "a = 3;", 1, UCL_DUPLICATE_APPEND,
		 "{\"a\":[1,2]}"},
		{"a = 1; a = 2;", 2, "a = 3;", 3, UCL_DUPLICATE_APPEND,
		 "{\"a\":3}"},
	};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct ucl_parser *p = ucl_parser_new(UCL_PARSER_NO_IMPLICIT_ARRAYS);
		assert(p != NULL);

		bool res = ucl_parser_add_chunk_full(p,
											 (const unsigned char *) cases[i].doc,
											 strlen(cases[i].doc),
											 cases[i].priority,
											 cases[i].strat, UCL_PARSE_UCL);

		if (res && cases[i].next != NULL) {
			res = ucl_parser_add_chunk_full(p,
											(const unsigned char *) cases[i].next,
											strlen(cases[i].next),
											cases[i].next_priority,
											cases[i].strat, UCL_PARSE_UCL);
		}

		if (!res) {
			fprintf(stderr, "'%s': %s\n", cases[i].doc, ucl_parser_get_error(p));
		}
		assert(res);

		ucl_object_t *obj = ucl_parser_get_object(p);
		ucl_parser_free(p);
		unsigned char *json = ucl_object_emit(obj, UCL_EMIT_JSON_COMPACT);
		assert(json != NULL);

		if (strcmp((const char *) json, cases[i].expect) != 0) {
			fprintf(stderr, "'%s': got %s, expected %s\n", cases[i].doc,
					(const char *) json, cases[i].expect);
		}
		assert(strcmp((const char *) json, cases[i].expect) == 0);

		free(json);
		ucl_object_unref(obj);
	}
}

/*
 * JSON and YAML write the values of a repeated key as an array. When the
 * first of them was an array itself, its elements were written instead, and
 * the other values were lost.
 */
static void
test_emit_repeated_key(void)
{
	static const struct {
		const char *doc;
		enum ucl_emitter type;
		const char *expect;
	} cases[] = {
		{"a [1, 2]; a = 5; b = 5; b [1, 2];", UCL_EMIT_JSON_COMPACT,
		 "{\"a\":[[1,2],5],\"b\":[5,[1,2]]}"},
		{"a [1, 2]; a = 5;", UCL_EMIT_YAML,
		 "a: [\n    [\n        1,\n        2\n    ],\n    5\n]"},
		{"a = 5; a = 6; b = 7; b = 8;", UCL_EMIT_YAML,
		 "a: [\n    5,\n    6\n]\nb: [\n    7,\n    8\n]"},
		{"a = 5; a = 6; b = 7; b = 8;", UCL_EMIT_JSON,
		 "{\n    \"a\": [\n        5,\n        6\n    ],\n"
		 "    \"b\": [\n        7,\n        8\n    ]\n}"},
	};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct ucl_parser *p = ucl_parser_new(0);
		assert(p != NULL);
		assert(ucl_parser_add_string(p, cases[i].doc, 0));

		ucl_object_t *obj = ucl_parser_get_object(p);
		ucl_parser_free(p);
		unsigned char *out = ucl_object_emit(obj, cases[i].type);
		assert(out != NULL);

		if (strcmp((const char *) out, cases[i].expect) != 0) {
			fprintf(stderr, "'%s': got '%s', expected '%s'\n", cases[i].doc,
					(const char *) out, cases[i].expect);
		}
		assert(strcmp((const char *) out, cases[i].expect) == 0);

		free(out);
		ucl_object_unref(obj);
	}
}

static char *
emit_doc(const char *doc, enum ucl_emitter type)
{
	struct ucl_parser *p = ucl_parser_new(0);
	assert(p != NULL);
	assert(ucl_parser_add_string(p, doc, 0));

	ucl_object_t *obj = ucl_parser_get_object(p);
	ucl_parser_free(p);
	unsigned char *out = ucl_object_emit(obj, type);
	assert(out != NULL);
	ucl_object_unref(obj);

	return (char *) out;
}

/*
 * The values of a repeated key must come out as the explicit array of the
 * same values would. The layout of that implicit array was chosen by the
 * length of its first value, so when that was 0, as for a number, an empty
 * string or an empty container, the brackets went out as those of an empty
 * array, on the lines of the values. YAML also got a comma before a repeated
 * key at the top level, where keys are separated by newlines alone.
 */
static void
test_emit_repeated_key_layout(void)
{
	static const struct {
		const char *implicit;
		const char *explicit;
	} cases[] = {
		{"a = 5; a = 6; a = 7; b = 8; b = 9;", "a [5, 6, 7]; b [8, 9];"},
		/* in an object, and after a key that is not repeated */
		{"s { a = 5; a = 6; b = 7; c = 8; c = 9; } t = 1; t = 2;",
		 "s { a [5, 6]; b = 7; c [8, 9]; } t [1, 2];"},
		/* first values that are containers, empty or not, or empty strings */
		{"a [1, 2]; a = 3; b { x = 1; } b = 4; c []; c = 5; d {} d = 6; "
		 "e = \"\"; e = 7;",
		 "a [[1, 2], 3]; b [{ x = 1; }, 4]; c [[], 5]; d [{}, 6]; "
		 "e [\"\", 7];"},
		/* among explicit arrays */
		{"a [1]; b = 2; b = 3; c [4, 5]; d = 6; d = 7; e [];",
		 "a [1]; b [2, 3]; c [4, 5]; d [6, 7]; e [];"},
	};
	static const enum ucl_emitter types[] = {
		UCL_EMIT_JSON, UCL_EMIT_JSON_COMPACT, UCL_EMIT_YAML};
	size_t i, j;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		for (j = 0; j < sizeof(types) / sizeof(types[0]); j++) {
			char *got = emit_doc(cases[i].implicit, types[j]);
			char *expect = emit_doc(cases[i].explicit, types[j]);

			if (strcmp(got, expect) != 0) {
				fprintf(stderr, "'%s' (emitter %d): got\n%s\nexpected\n%s\n",
						cases[i].implicit, (int) types[j], got, expect);
			}
			assert(strcmp(got, expect) == 0);

			free(got);
			free(expect);
		}
	}
}

/*
 * With UCL_DUPLICATE_MERGE, a value is merged into the old container of its
 * key only if it is a container itself. The parser carries its state from a
 * chunk to the next one, so the value may start in a later chunk than its key,
 * and nothing tells what it is until it does. Taken for a scalar, a container
 * formed an implicit array with the old one rather than being merged into it,
 * did not report a mismatch, and at a lower priority was dropped, leaking all
 * it held.
 */
static void
test_merge_split_value(void)
{
	static const struct {
		const char *chunks[3];
		unsigned int priorities[3];
		const char *expect; /* NULL if the last chunk must fail */
	} cases[] = {
		{{"a {x = 1}\n", "a =\n", "{y {z = 1}}"}, {2, 1, 1},
		 "{\"a\":{\"x\":1,\"y\":{\"z\":1}}}"},
		{{"a {x = 1}\n", "a =\n", "{y {z = 1}}"}, {0, 0, 0},
		 "{\"a\":{\"x\":1,\"y\":{\"z\":1}}}"},
		{{"a [1]\n", "a =\n# comment\n", "[2]"}, {0, 0, 0},
		 "{\"a\":[1,2]}"},
		{{"a {x = 1}\n", "a =\n", "[1]"}, {0, 0, 0}, NULL},
		/* A scalar follows the priorities */
		{{"a [1, 2]\n", "a =\n", "5"}, {0, 0, 0}, "{\"a\":[[1,2],5]}"},
		{{"a [1, 2]\n", "a =\n", "5"}, {2, 1, 1}, "{\"a\":[1,2]}"},
		{{"a [1, 2]\n", "a =\n", "5"}, {1, 2, 2}, "{\"a\":5}"},
		/* The value never comes */
		{{"a [1, 2]\n", "a =\n", NULL}, {0, 0, 0}, "{\"a\":[1,2]}"},
	};
	size_t i, j;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct ucl_parser *p = ucl_parser_new(0);
		bool res = true;
		assert(p != NULL);

		for (j = 0; j < 3 && cases[i].chunks[j] != NULL && res; j++) {
			res = ucl_parser_add_chunk_full(p,
											(const unsigned char *) cases[i].chunks[j],
											strlen(cases[i].chunks[j]),
											cases[i].priorities[j],
											UCL_DUPLICATE_MERGE, UCL_PARSE_UCL);
		}

		if (cases[i].expect == NULL) {
			assert(!res && j == 3);
			assert(ucl_parser_get_error_code(p) == UCL_EMERGE);
			assert(strstr(ucl_parser_get_error(p),
						  "cannot merge an object with an array") != NULL);
			ucl_parser_free(p);
			continue;
		}

		if (!res) {
			fprintf(stderr, "case %zu: %s\n", i, ucl_parser_get_error(p));
		}
		assert(res);

		ucl_object_t *obj = ucl_parser_get_object(p);
		ucl_parser_free(p);
		unsigned char *json = ucl_object_emit(obj, UCL_EMIT_JSON_COMPACT);
		assert(json != NULL);

		if (strcmp((const char *) json, cases[i].expect) != 0) {
			fprintf(stderr, "case %zu: got %s, expected %s\n", i,
					(const char *) json, cases[i].expect);
		}
		assert(strcmp((const char *) json, cases[i].expect) == 0);

		free(json);
		ucl_object_unref(obj);
	}
}

/*
 * A chunk inserted between a key and its value, as a macro can do, adds a
 * key before the merge left pending for the first one is settled. That merge
 * is dropped, which leaves the old container as the value of its key. Settled
 * later instead, it took the value of the inserted key, which became null.
 */
static void
test_merge_pending_insert(void)
{
	static const char first[] = "a {x = 1}\na =\n";
	static const char inserted[] = "b = 2";
	struct ucl_parser *p = ucl_parser_new(0);
	ucl_object_t *obj;
	unsigned char *json;

	assert(p != NULL);
	assert(ucl_parser_add_chunk_full(p, (const unsigned char *) first,
									 sizeof(first) - 1, 0,
									 UCL_DUPLICATE_MERGE, UCL_PARSE_UCL));
	assert(ucl_parser_insert_chunk(p, (const unsigned char *) inserted,
								   sizeof(inserted) - 1));

	obj = ucl_parser_get_object(p);
	ucl_parser_free(p);
	json = ucl_object_emit(obj, UCL_EMIT_JSON_COMPACT);
	assert(json != NULL);
	if (strcmp((const char *) json, "{\"a\":{\"x\":1},\"b\":2}") != 0) {
		fprintf(stderr, "pending insert: got %s\n", (const char *) json);
	}
	assert(strcmp((const char *) json, "{\"a\":{\"x\":1},\"b\":2}") == 0);

	free(json);
	ucl_object_unref(obj);
}

/*
 * The comments before a key whose value starts in a later chunk belong to
 * that value, as in a single chunk. With the merge pending at the end of
 * the chunk, they went to the old container as trailing comments, and so
 * were written after it, above the next key.
 */
static char *
emit_merged_with_comments(const char *const *chunks, size_t nchunks)
{
	struct ucl_parser *p = ucl_parser_new(UCL_PARSER_SAVE_COMMENTS);
	struct ucl_emitter_functions *func;
	ucl_object_t *obj;
	char *out = NULL;
	size_t i;

	assert(p != NULL);
	for (i = 0; i < nchunks; i++) {
		assert(ucl_parser_add_chunk_full(p, (const unsigned char *) chunks[i],
										 strlen(chunks[i]), 0,
										 UCL_DUPLICATE_MERGE, UCL_PARSE_UCL));
	}

	obj = ucl_parser_get_object(p);
	assert(obj != NULL);
	func = ucl_object_emit_memory_funcs((void **) &out);
	assert(func != NULL);
	assert(ucl_object_emit_full(obj, UCL_EMIT_CONFIG, func,
								ucl_parser_get_comments(p)));
	ucl_object_emit_funcs_free(func);
	ucl_object_unref(obj);
	ucl_parser_free(p);

	return out;
}

static void
test_merge_split_comment(void)
{
	static const char *const split[] = {"a {x = 1}\n", "# about a\na =\n",
										"{y = 1}\nb = 2\n"};
	static const char *const whole[] = {"a {x = 1}\n# about a\na =\n{y = 1}\nb = 2\n"};
	char *got = emit_merged_with_comments(split, 3),
		 *expect = emit_merged_with_comments(whole, 1);

	assert(got != NULL && expect != NULL);
	assert(strstr(expect, "# about a\na {") != NULL);
	if (strcmp(got, expect) != 0) {
		fprintf(stderr, "split comment: got '%s', expected '%s'\n", got, expect);
	}
	assert(strcmp(got, expect) == 0);

	free(got);
	free(expect);
}

/*
 * A value of a higher priority replaces the old value of its key, which is
 * released. The comments of all it held stayed with the parser, keyed by
 * addresses that objects allocated later reused, and so were written above
 * unrelated keys. Its own comment, which belongs to the key, was lost.
 */
static void
test_replace_comments(void)
{
	static const char base[] = "# about sec\nsec {\n# about a\na {\n# about x\n"
							   "x = 1\n}\n# about b\nb [\n# about el\n1\n]\n}\n";
	static const char over[] = "sec = 5\nn1 = 1\nn2 = 2\nn3 {\ny = 1\n}\n";
	static const char expect[] = "# about sec\nsec = 5;\nn1 = 1;\nn2 = 2;\n"
								 "n3 {\n    y = 1;\n}\n";
	static const enum ucl_duplicate_strategy strategies[] = {
		UCL_DUPLICATE_MERGE, UCL_DUPLICATE_APPEND, UCL_DUPLICATE_REWRITE};
	size_t i;

	for (i = 0; i < sizeof(strategies) / sizeof(strategies[0]); i++) {
		struct ucl_parser *p = ucl_parser_new(UCL_PARSER_SAVE_COMMENTS);
		struct ucl_emitter_functions *func;
		const ucl_object_t *comments, *comment;
		ucl_object_iter_t it = NULL;
		ucl_object_t *obj;
		char *out = NULL;
		unsigned int n = 0;

		assert(p != NULL);
		assert(ucl_parser_add_chunk_full(p, (const unsigned char *) base,
										 sizeof(base) - 1, 0, strategies[i],
										 UCL_PARSE_UCL));
		assert(ucl_parser_add_chunk_full(p, (const unsigned char *) over,
										 sizeof(over) - 1, 1, strategies[i],
										 UCL_PARSE_UCL));
		obj = ucl_parser_get_object(p);
		assert(obj != NULL);

		comments = ucl_parser_get_comments(p);
		while (ucl_object_iterate(comments, &it, true) != NULL) {
			n++;
		}
		assert(n == 1);
		comment = ucl_comments_find(comments, ucl_object_lookup(obj, "sec"));
		assert(comment != NULL);
		assert(strcmp(ucl_object_tostring(comment), "# about sec") == 0);

		func = ucl_object_emit_memory_funcs((void **) &out);
		assert(func != NULL);
		assert(ucl_object_emit_full(obj, UCL_EMIT_CONFIG, func, comments));
		ucl_object_emit_funcs_free(func);
		if (strcmp(out, expect) != 0) {
			fprintf(stderr, "strategy %d: got '%s', expected '%s'\n",
					(int) strategies[i], out, expect);
		}
		assert(strcmp(out, expect) == 0);

		free(out);
		ucl_object_unref(obj);
		ucl_parser_free(p);
	}
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	test_zerocopy_duplicate_error();
	test_binary_duplicate_error();
	test_merge_no_implicit_arrays();
	test_no_implicit_arrays_priority();
	test_emit_repeated_key();
	test_emit_repeated_key_layout();
	test_merge_split_value();
	test_merge_pending_insert();
	test_merge_split_comment();
	test_replace_comments();

	printf("All duplicate-key error tests passed\n");

	return 0;
}
