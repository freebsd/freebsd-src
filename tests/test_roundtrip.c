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
 * Round-trip regression test for UCL_EMIT_CONFIG.
 *
 * Each case builds an object programmatically, emits it as UCL_EMIT_CONFIG,
 * re-parses the emitted text, and asserts that the re-parsed object is
 * structurally equal to the original via ucl_object_compare().
 *
 * Originally added to cover the emitter-escaping bugs fixed in PR #373:
 *   - embedded "\nEOD\n" in a heredoc-eligible string prematurely closed
 *     the heredoc block on re-parse
 *   - a literal "\'" in a squoted string was misinterpreted on re-parse
 *
 * New cases can be added without any .res files — the test is
 * self-validating.
 *
 * Text cases parse UCL text with comments saved instead, and round-trip it
 * with the comments emitted, as configuration dumps that keep comments do.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ucl.h"

struct roundtrip_case {
	const char *name;
	/* Builds and returns the object to round-trip. */
	ucl_object_t *(*build)(void);
};

/* Helper: insert a string value with a given length (so we can include NULs
 * or embedded newlines without C-string terminator ambiguity). */
static void
insert_lstring(ucl_object_t *obj, const char *key, const char *val, size_t len)
{
	ucl_object_t *cur = ucl_object_fromlstring(val, len);
	ucl_object_insert_key(obj, cur, key, 0, true);
}

/* Case 1: multiline string with "\nEOD\n" embedded — pre-fix this broke
 * the heredoc block on re-parse. */
static ucl_object_t *
build_heredoc_eod_middle(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	const char v[] =
		"a string that is longer than eighty characters so that "
		"the emitter picks heredoc mode\nEOD\ntrailing content";
	insert_lstring(obj, "k", v, sizeof(v) - 1);
	return obj;
}

/* Case 2: heredoc-eligible string whose embedded EOD sits at the very end
 * (no trailing newline after it). Exercises the boundary condition in the
 * emitter's scan loop. */
static ucl_object_t *
build_heredoc_eod_tail(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	const char v[] =
		"another long string padded so the emitter selects multiline "
		"heredoc output for sure\nEOD";
	insert_lstring(obj, "k", v, sizeof(v) - 1);
	return obj;
}

/* Case 3: heredoc-eligible string with the letters "EOD" that are NOT
 * preceded by a newline — must still round-trip as a heredoc (i.e., the
 * fallback must NOT fire on false positives). */
static ucl_object_t *
build_heredoc_eod_substring(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	const char v[] =
		"plenty of content to exceed the long-string threshold so "
		"heredoc mode is picked\nsomething EOD here not at line start";
	insert_lstring(obj, "k", v, sizeof(v) - 1);
	return obj;
}

/* Case 4: normal multiline heredoc without any EOD — regression guard that
 * heredoc emission still works in the common case. */
static ucl_object_t *
build_heredoc_normal(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	const char v[] =
		"first line of a perfectly ordinary multiline string\n"
		"second line\n"
		"third line containing enough bytes to exceed the threshold";
	insert_lstring(obj, "k", v, sizeof(v) - 1);
	return obj;
}

/* Case 5: squoted string with literal "\'" — pre-fix the parser treated
 * this as an escape sequence and dropped the backslash on re-parse. */
static ucl_object_t *
build_squote_backslash_quote(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	ucl_object_t *cur = ucl_object_fromstring("value with \\' inside");
	cur->flags |= UCL_OBJECT_SQUOTED;
	ucl_object_insert_key(obj, cur, "k", 0, true);
	return obj;
}

/* Case 6: squoted string with a single quote and no backslash — regression
 * guard for normal squoted output. */
static ucl_object_t *
build_squote_simple(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	ucl_object_t *cur = ucl_object_fromstring("can't stop won't stop");
	cur->flags |= UCL_OBJECT_SQUOTED;
	ucl_object_insert_key(obj, cur, "k", 0, true);
	return obj;
}

/* Case 7: multiple backslashes adjacent to quotes and newlines, combining
 * both fix paths in one object. */
static ucl_object_t *
build_combined(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);

	/* long string with embedded EOD */
	const char v1[] =
		"combined case: first a long heredoc-eligible payload "
		"so the multiline branch is taken\nEOD\nmiddle\nEOD\nend";
	insert_lstring(obj, "multiline", v1, sizeof(v1) - 1);

	/* short squoted string with backslash-quote */
	ucl_object_t *sq = ucl_object_fromstring("\\'");
	sq->flags |= UCL_OBJECT_SQUOTED;
	ucl_object_insert_key(obj, sq, "squoted", 0, true);

	/* plain nested object for good measure */
	ucl_object_t *nested = ucl_object_typed_new(UCL_OBJECT);
	ucl_object_insert_key(nested, ucl_object_fromint(42), "answer", 0, true);
	ucl_object_insert_key(obj, nested, "nested", 0, true);

	return obj;
}

/* Case 8: very simple scalar object — baseline sanity check. */
static ucl_object_t *
build_simple(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	ucl_object_insert_key(obj, ucl_object_fromstring("hello"), "greeting", 0, true);
	ucl_object_insert_key(obj, ucl_object_fromint(123), "number", 0, true);
	ucl_object_insert_key(obj, ucl_object_frombool(true), "flag", 0, true);
	return obj;
}

static const struct roundtrip_case cases[] = {
	{"simple",                   build_simple},
	{"heredoc_normal",           build_heredoc_normal},
	{"heredoc_eod_middle",       build_heredoc_eod_middle},
	{"heredoc_eod_tail",         build_heredoc_eod_tail},
	{"heredoc_eod_substring",    build_heredoc_eod_substring},
	{"squote_simple",            build_squote_simple},
	{"squote_backslash_quote",   build_squote_backslash_quote},
	{"combined",                 build_combined},
};

static int
run_case(const struct roundtrip_case *tc)
{
	ucl_object_t *orig = tc->build();
	if (orig == NULL) {
		fprintf(stderr, "[%s] FAIL: build returned NULL\n", tc->name);
		return 1;
	}

	unsigned char *emitted = ucl_object_emit(orig, UCL_EMIT_CONFIG);
	if (emitted == NULL) {
		fprintf(stderr, "[%s] FAIL: emit returned NULL\n", tc->name);
		ucl_object_unref(orig);
		return 1;
	}

	struct ucl_parser *parser = ucl_parser_new(0);
	if (!ucl_parser_add_string(parser, (const char *)emitted, 0) ||
			ucl_parser_get_error(parser) != NULL) {
		fprintf(stderr, "[%s] FAIL: re-parse error: %s\nemitted was:\n%s\n",
				tc->name,
				ucl_parser_get_error(parser)
					? ucl_parser_get_error(parser)
					: "(unknown)",
				emitted);
		ucl_parser_free(parser);
		free(emitted);
		ucl_object_unref(orig);
		return 1;
	}

	ucl_object_t *reparsed = ucl_parser_get_object(parser);
	ucl_parser_free(parser);

	int cmp = ucl_object_compare(orig, reparsed);
	int rc = 0;
	if (cmp != 0) {
		fprintf(stderr,
				"[%s] FAIL: round-trip object mismatch (compare=%d)\n"
				"emitted was:\n%s\n",
				tc->name, cmp, emitted);
		rc = 1;
	}
	else {
		/* Second round-trip: emit the re-parsed object and verify the
		 * emitter is idempotent (the text is identical to the first
		 * emit). This catches emitters whose output depends on some
		 * flag that was stripped by the parser. */
		unsigned char *emitted2 = ucl_object_emit(reparsed, UCL_EMIT_CONFIG);
		if (emitted2 == NULL) {
			fprintf(stderr, "[%s] FAIL: second emit returned NULL\n",
					tc->name);
			rc = 1;
		}
		else if (strcmp((const char *)emitted, (const char *)emitted2) != 0) {
			fprintf(stderr,
					"[%s] FAIL: emit not idempotent\nfirst:\n%s\nsecond:\n%s\n",
					tc->name, emitted, emitted2);
			rc = 1;
		}
		free(emitted2);
	}

	if (rc == 0) {
		printf("[%s] ok\n", tc->name);
	}

	free(emitted);
	ucl_object_unref(orig);
	ucl_object_unref(reparsed);
	return rc;
}

/*
 * Round trips of parsed text with its comments saved and emitted. The input
 * must parse into the object of `expect` (JSON), with and without
 * UCL_PARSER_SAVE_COMMENTS. Its UCL_EMIT_CONFIG output, comments included,
 * must be `emit` when that is set, parse back into the same object and emit
 * the same text again.
 */
struct text_case {
	const char *name;
	int flags;
	const char *input;
	const char *expect;
	const char *emit;
};

static const struct text_case text_cases[] = {
	/* A comment before the first value of an array, indented as emitted */
	{"comment_first_in_array", 0,
	 "a [\n    # first\n    1,\n    2\n]\n",
	 "{\"a\": [1, 2]}",
	 "a [\n    # first\n    1,\n    2,\n]\n"},
	{"comments_first_in_array", 0,
	 "a = [\n  # one\n\n  /* two */\n  \"x\",\n  \"y\"\n]\n",
	 "{\"a\": [\"x\", \"y\"]}",
	 "a [\n    # one\n    /* two */\n    \"x\",\n    \"y\",\n]\n"},
	{"comment_first_in_top_array", 0,
	 "[\n  # first\n  1\n]\n",
	 "[1]",
	 NULL},
	/* A C comment is saved without the character that follows it */
	{"c_comment_before_value", 0,
	 "a =\n/* c */{\n    b = 1;\n}\n",
	 "{\"a\": {\"b\": 1}}",
	 NULL},
	{"c_comment_before_key", 0,
	 "a = 1; /* c */ b = 2;\n",
	 "{\"a\": 1, \"b\": 2}",
	 "a = 1;\n/* c */\nb = 2;\n"},
	{"c_comment_at_end", 0,
	 "a = 1; /* c */",
	 "{\"a\": 1}",
	 "a = 1;\n/* c */\n"},
	/*
	 * Macros are comments with UCL_PARSER_DISABLE_MACRO: they are skipped
	 * wherever they may be, and saved as they are written
	 */
	{"macros_as_comments", UCL_PARSER_DISABLE_MACRO,
	 ".include \"${CURDIR}/x.conf\"\n"
	 "a = 1;\n"
	 "sec {\n    .inherit \"a\"\n    .priority 2\n    b = 2;\n}\n"
	 ".include(glob=true, key=\"a)b\") \"y\"\n"
	 ".priority 1\n"
	 "c = 3;\n",
	 "{\"a\": 1, \"sec\": {\"b\": 2}, \"c\": 3}",
	 ".include \"${CURDIR}/x.conf\"\n"
	 "a = 1;\n"
	 "sec {\n    .inherit \"a\"\n    .priority 2\n    b = 2;\n}\n"
	 ".include(glob=true, key=\"a)b\") \"y\"\n"
	 ".priority 1\n"
	 "c = 3;\n"},
	{"macro_forms_as_comments", UCL_PARSER_DISABLE_MACRO,
	 ".custom {\n  any text, even \"quotes\n}\n"
	 ".priority 2; a = 1\n"
	 ".include\n  \"x\"\n"
	 "b = 2\n",
	 "{\"a\": 1, \"b\": 2}",
	 ".custom {\n  any text, even \"quotes\n}\n"
	 ".priority 2\n"
	 "a = 1;\n"
	 ".include\n  \"x\"\n"
	 "b = 2;\n"},
	{"macro_in_array_as_comment", UCL_PARSER_DISABLE_MACRO,
	 "arr [\n  {\n    .inherit \"x\"\n    a = 1;\n  }\n]\n",
	 "{\"arr\": [{\"a\": 1}]}",
	 "arr [\n    {\n        .inherit \"x\"\n        a = 1;\n    }\n]\n"},
	{"macro_at_end_as_comment", UCL_PARSER_DISABLE_MACRO,
	 "a = 1;\n.include \"x\"",
	 "{\"a\": 1}",
	 "a = 1;\n.include \"x\"\n"},
	/*
	 * Comments after a container follow it, at its level, not the last
	 * value inside it, where a macro is no macro
	 */
	{"macro_after_container_as_comment", UCL_PARSER_DISABLE_MACRO,
	 "a [1]\n.priority 1\n",
	 "{\"a\": [1]}",
	 "a [\n    1,\n]\n.priority 1\n"},
	{"comments_after_containers", 0,
	 "a {\n    b [1]\n    # c\n}\n# d\n",
	 "{\"a\": {\"b\": [1]}}",
	 "a {\n    b [\n        1,\n    ]\n    # c\n}\n# d\n"},
	{"comments_around_container", 0,
	 "# before\na {\n    b = 1;\n}\n# after\n",
	 "{\"a\": {\"b\": 1}}",
	 "# before\na {\n    b = 1;\n}\n# after\n"},
	{"comments_after_last_values", 0,
	 "a {\n    x = 1;\n    y = 2; # after y\n    # more\n}\nb [\n    1,\n    2 # after 2\n]\n",
	 "{\"a\": {\"x\": 1, \"y\": 2}, \"b\": [1, 2]}",
	 "a {\n    x = 1;\n    y = 2;\n    # after y\n    # more\n}\nb [\n    1,\n    2,\n    # after 2\n]\n"},
	/*
	 * Comments inside an empty container stay inside it, rather than follow
	 * it, where a macro would no longer be inside
	 */
	{"comments_in_empty_containers", UCL_PARSER_DISABLE_MACRO,
	 "# lead\nsec {\n  # c\n  .include \"x\"\n} # after\n"
	 "a {\n  e {}\n  # after e\n}\n"
	 "b [ /* in b */ ]\n"
	 "c { d {\n.priority 1\n} }\n",
	 "{\"sec\": {}, \"a\": {\"e\": {}}, \"b\": [], \"c\": {\"d\": {}}}",
	 "# lead\nsec {\n    # c\n    .include \"x\"\n}\n# after\n"
	 "a {\n    e {}\n    # after e\n}\n"
	 "b [\n    /* in b */\n]\n"
	 "c {\n    d {\n        .priority 1\n    }\n}\n"},
	/* The top object is written without braces, so are its comments */
	{"comments_in_empty_top_object", UCL_PARSER_DISABLE_MACRO,
	 "{\n  # c\n  .include \"x\"\n}\n",
	 "{}",
	 "# c\n.include \"x\"\n"},
	{"comments_in_empty_top_array", 0,
	 "[\n  # c\n]\n",
	 "[]",
	 "[\n    # c\n]"},
	/* A number ends where a comment starts, as at the end of any atom */
	{"comments_after_numbers", 0,
	 "a = 1 /* c */\nb = 2/* d */;\nc = 1kb # e\nd = 0x10 /* f */\n"
	 "e [1 /* g */, 2.5/* h */]\n",
	 "{\"a\": 1, \"b\": 2, \"c\": 1024, \"d\": 16, \"e\": [1, 2.5]}",
	 NULL},
};

/*
 * Parse text from a buffer of its exact size, so that AddressSanitizer
 * catches any read past it
 */
static ucl_object_t *
parse_text(const char *name, const char *text, int flags,
		   ucl_object_t **comments)
{
	size_t len = strlen(text);
	unsigned char *buf = malloc(len);
	struct ucl_parser *parser;
	ucl_object_t *obj = NULL;

	if (buf == NULL) {
		return NULL;
	}

	memcpy(buf, text, len);
	parser = ucl_parser_new(flags);

	if (parser == NULL) {
		free(buf);
		return NULL;
	}

	if (!ucl_parser_add_chunk(parser, buf, len) ||
		ucl_parser_get_error(parser) != NULL) {
		fprintf(stderr, "[%s] FAIL: parse error: %s\ntext was:\n%s\n", name,
				ucl_parser_get_error(parser) ? ucl_parser_get_error(parser)
											 : "(unknown)",
				text);
	}
	else {
		obj = ucl_parser_get_object(parser);

		if (comments != NULL && ucl_parser_get_comments(parser) != NULL) {
			*comments = ucl_object_ref(ucl_parser_get_comments(parser));
		}
	}

	ucl_parser_free(parser);
	free(buf);

	return obj;
}

static char *
emit_text(const ucl_object_t *obj, const ucl_object_t *comments)
{
	struct ucl_emitter_functions *func;
	char *out = NULL;

	func = ucl_object_emit_memory_funcs((void **) &out);

	if (func == NULL) {
		return NULL;
	}

	if (!ucl_object_emit_full(obj, UCL_EMIT_CONFIG, func, comments)) {
		free(out);
		out = NULL;
	}

	ucl_object_emit_funcs_free(func);

	return out;
}

static int
run_text_case(const struct text_case *tc)
{
	ucl_object_t *expect, *plain, *obj, *reparsed = NULL, *comments = NULL,
						  *comments2 = NULL;
	char *emitted = NULL, *emitted2 = NULL;
	unsigned char *json;
	int rc = 1;

	expect = parse_text(tc->name, tc->expect, 0, NULL);
	plain = parse_text(tc->name, tc->input, tc->flags, NULL);
	obj = parse_text(tc->name, tc->input,
					 tc->flags | UCL_PARSER_SAVE_COMMENTS, &comments);

	if (expect == NULL || plain == NULL || obj == NULL) {
		goto out;
	}

	if (ucl_object_compare(plain, expect) != 0 ||
		ucl_object_compare(obj, expect) != 0) {
		json = ucl_object_emit(ucl_object_compare(plain, expect) != 0 ? plain : obj,
							   UCL_EMIT_JSON_COMPACT);
		fprintf(stderr, "[%s] FAIL: parsed %s, expected %s\n", tc->name,
				json ? (const char *) json : "(null)", tc->expect);
		free(json);
		goto out;
	}

	emitted = emit_text(obj, comments);

	if (emitted == NULL) {
		fprintf(stderr, "[%s] FAIL: emit returned NULL\n", tc->name);
		goto out;
	}

	if (tc->emit != NULL && strcmp(emitted, tc->emit) != 0) {
		fprintf(stderr, "[%s] FAIL: emitted:\n%s\nexpected:\n%s\n", tc->name,
				emitted, tc->emit);
		goto out;
	}

	reparsed = parse_text(tc->name, emitted,
						  tc->flags | UCL_PARSER_SAVE_COMMENTS, &comments2);

	if (reparsed == NULL) {
		goto out;
	}

	if (ucl_object_compare(reparsed, expect) != 0) {
		fprintf(stderr, "[%s] FAIL: round-trip object mismatch\n"
						"emitted was:\n%s\n",
				tc->name, emitted);
		goto out;
	}

	emitted2 = emit_text(reparsed, comments2);

	if (emitted2 == NULL || strcmp(emitted, emitted2) != 0) {
		fprintf(stderr, "[%s] FAIL: emit not idempotent\nfirst:\n%s\nsecond:\n%s\n",
				tc->name, emitted, emitted2 ? emitted2 : "(null)");
		goto out;
	}

	printf("[%s] ok\n", tc->name);
	rc = 0;

out:
	free(emitted);
	free(emitted2);
	ucl_object_unref(expect);
	ucl_object_unref(plain);
	ucl_object_unref(obj);
	ucl_object_unref(reparsed);
	ucl_object_unref(comments);
	ucl_object_unref(comments2);

	return rc;
}

int
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	int failures = 0;
	size_t ncases = sizeof(cases) / sizeof(cases[0]),
		   ntext = sizeof(text_cases) / sizeof(text_cases[0]);

	for (size_t i = 0; i < ncases; i++) {
		failures += run_case(&cases[i]);
	}

	for (size_t i = 0; i < ntext; i++) {
		failures += run_text_case(&text_cases[i]);
	}

	if (failures != 0) {
		fprintf(stderr, "%d/%zu round-trip cases failed\n", failures,
				ncases + ntext);
		return 1;
	}

	printf("all %zu round-trip cases passed\n", ncases + ntext);
	return 0;
}
