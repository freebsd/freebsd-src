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
 * Regression tests for $ref cycle detection when a schema object is shared
 * between the root document and an external reference. Local refs inside
 * such a schema resolve against whichever root it was reached under, so
 * reaching it again under another root is not a cycle. The JSON suites in
 * tests/schema cannot express shared objects, hence the C API here.
 *
 * It also covers malformed schemas, where the exact error code matters,
 * binary strings, which only msgpack input can produce, and references to
 * files, which need one to exist.
 */

#include "ucl.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

static ucl_object_t *
parse(const char *str)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	ucl_object_t *obj;

	assert(parser != NULL);
	assert(ucl_parser_add_string(parser, str, 0));
	obj = ucl_parser_get_object(parser);
	assert(obj != NULL);
	ucl_parser_free(parser);

	return obj;
}

static ucl_object_t *
parse_msgpack(const char *data, size_t len)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	ucl_object_t *obj;

	assert(parser != NULL);
	assert(ucl_parser_add_chunk_full(parser, (const unsigned char *) data, len,
									 0, UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK));
	obj = ucl_parser_get_object(parser);
	assert(obj != NULL);
	ucl_parser_free(parser);

	return obj;
}

static bool
validate(const ucl_object_t *root, ucl_object_t *ext, const char *data,
		 struct ucl_schema_error *err)
{
	ucl_object_t *doc = parse(data);
	bool ret;

	memset(err, 0, sizeof(*err));
	ret = ucl_object_validate_root_ext(root, ucl_object_lookup(doc, "v"),
									   root, ext, err);
	ucl_object_unref(doc);

	return ret;
}

/*
 * S is reached first under root R, where #/a leads to a ref back into SUB,
 * then under root SUB, where #/a is a plain type check.
 */
static void
test_shared_subschema(void)
{
	struct ucl_schema_error err;
	ucl_object_t *sub, *root, *ext;

	sub = parse("{\"s\": {\"$ref\": \"#/a\"}, \"a\": {\"type\": \"integer\"}}");
	root = parse("{\"$ref\": \"#/sub/s\", \"a\": {\"$ref\": \"sub#/s\"}}");
	ext = ucl_object_typed_new(UCL_OBJECT);
	assert(ucl_object_insert_key(root, ucl_object_ref(sub), "sub", 0, false));
	assert(ucl_object_insert_key(ext, ucl_object_ref(sub), "sub", 0, false));

	assert(validate(root, ext, "{\"v\": 1}", &err));
	assert(!validate(root, ext, "{\"v\": \"x\"}", &err));
	assert(err.code == UCL_SCHEMA_TYPE_MISMATCH);

	ucl_object_unref(ext);
	ucl_object_unref(root);
	ucl_object_unref(sub);
}

/*
 * A real cycle that alternates between the two roots must still be
 * reported: S under R -> R.t -> S under SUB -> SUB.t -> S under R.
 */
static void
test_cross_root_cycle(void)
{
	struct ucl_schema_error err;
	ucl_object_t *sub, *root, *ext;

	sub = parse("{\"s\": {\"$ref\": \"#/t\"}, \"t\": {\"$ref\": \"r#/sub/s\"}}");
	root = parse("{\"$ref\": \"#/sub/s\", \"t\": {\"$ref\": \"sub#/s\"}}");
	ext = ucl_object_typed_new(UCL_OBJECT);
	assert(ucl_object_insert_key(root, ucl_object_ref(sub), "sub", 0, false));
	assert(ucl_object_insert_key(ext, ucl_object_ref(sub), "sub", 0, false));
	assert(ucl_object_insert_key(ext, ucl_object_ref(root), "r", 0, false));

	assert(!validate(root, ext, "{\"v\": 1}", &err));
	assert(err.code == UCL_SCHEMA_INVALID_SCHEMA);
	assert(strstr(err.msg, "cyclic reference") != NULL);

	ucl_object_unref(ext);
	ucl_object_unref(root);
	ucl_object_unref(sub);
}

/*
 * A $ref without a C string value, either not a string at all or a binary
 * string, is an invalid schema rather than a NULL reference to resolve.
 */
static void
test_non_string_ref(void)
{
	static const char *const refs[] = {"1", "null", "true", "{}", "[\"#\"]"};
	/* {"$ref": <bin "#">} */
	static const char mp_ref[] = "\x81\xa4$ref\xc4\x01#";
	struct ucl_schema_error err;
	ucl_object_t *schema;
	char buf[64];
	unsigned int i;

	for (i = 0; i < sizeof(refs) / sizeof(refs[0]); i++) {
		snprintf(buf, sizeof(buf), "{\"$ref\": %s}", refs[i]);
		schema = parse(buf);
		assert(!validate(schema, NULL, "{\"v\": 1}", &err));
		assert(err.code == UCL_SCHEMA_INVALID_SCHEMA);
		assert(err.obj == ucl_object_lookup(schema, "$ref"));
		ucl_object_unref(schema);
	}

	schema = parse_msgpack(mp_ref, sizeof(mp_ref) - 1);
	assert(ucl_object_lookup(schema, "$ref")->flags & UCL_OBJECT_BINARY);
	assert(!validate(schema, NULL, "{\"v\": 1}", &err));
	assert(err.code == UCL_SCHEMA_INVALID_SCHEMA);
	assert(err.obj == ucl_object_lookup(schema, "$ref"));
	ucl_object_unref(schema);
}

/*
 * Keywords whose value has the wrong type, or is a binary string, make the
 * schema invalid instead of reaching the validator as a NULL key or string.
 * So does an empty anyOf or oneOf, which nothing can satisfy.
 */
static void
test_malformed_keywords(void)
{
	static const struct {
		const char *schema;
		const char *data;
	} cases[] = {
		{"{\"type\": 1}", "{\"v\": 1}"},
		{"{\"type\": null}", "{\"v\": 1}"},
		{"{\"patternProperties\": [{\"type\": \"integer\"}]}",
		 "{\"v\": {\"a\": 1}}"},
		/* Repeated, the array used to reach regcomp() as keyless items */
		{"{\"patternProperties\": {\"^x\": {}}, \"patternProperties\": [{}], "
		 "\"additionalProperties\": false}",
		 "{\"v\": {\"y\": 1}}"},
		{"{\"required\": [1]}", "{\"v\": {\"a\": 1}}"},
		{"{\"dependencies\": {\"a\": [1]}}", "{\"v\": {\"a\": 1}}"},
		/* Nothing to match, which failed without an error */
		{"{\"anyOf\": []}", "{\"v\": 1}"},
		{"{\"allOf\": [{}], \"anyOf\": []}", "{\"v\": 1}"},
		{"{\"oneOf\": []}", "{\"v\": 1}"},
	};
	/* {"type": <bin "integer">} */
	static const char mp_type[] = "\x81\xa4type\xc4\x07integer";
	/* {"pattern": <bin "x">} */
	static const char mp_pattern[] = "\x81\xa7pattern\xc4\x01x";
	struct ucl_schema_error err;
	ucl_object_t *schema;
	unsigned int i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		schema = parse(cases[i].schema);
		assert(!validate(schema, NULL, cases[i].data, &err));
		assert(err.code == UCL_SCHEMA_INVALID_SCHEMA);
		/* No NULL string made it into the message */
		assert(strstr(err.msg, "(null)") == NULL);
		ucl_object_unref(schema);
	}

	schema = parse_msgpack(mp_type, sizeof(mp_type) - 1);
	assert(ucl_object_lookup(schema, "type")->flags & UCL_OBJECT_BINARY);
	assert(!validate(schema, NULL, "{\"v\": 1}", &err));
	assert(err.code == UCL_SCHEMA_INVALID_SCHEMA);
	ucl_object_unref(schema);

	schema = parse_msgpack(mp_pattern, sizeof(mp_pattern) - 1);
	assert(ucl_object_lookup(schema, "pattern")->flags & UCL_OBJECT_BINARY);
	assert(!validate(schema, NULL, "{\"v\": \"x\"}", &err));
	assert(err.code == UCL_SCHEMA_INVALID_SCHEMA);
	assert(strstr(err.msg, "(null)") == NULL);
	ucl_object_unref(schema);
}

/*
 * A "type" array that is empty, or has anything but a type name in it,
 * makes the schema invalid, even if the object matches another entry.
 */
static void
test_malformed_type_array(void)
{
	static const char *const types[] = {
		"[]",
		"[1, \"integer\"]",
		"[\"integer\", null]",
		"[\"integer\", \"foo\"]",
		"[[\"integer\"]]",
	};
	/* {"type": [<bin "integer">, "integer"]} */
	static const char mp_type[] = "\x81\xa4type\x92\xc4\x07integer\xa7integer";
	struct ucl_schema_error err;
	ucl_object_t *schema;
	char buf[64];
	unsigned int i;

	for (i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		snprintf(buf, sizeof(buf), "{\"type\": %s}", types[i]);
		schema = parse(buf);
		assert(!validate(schema, NULL, "{\"v\": 1}", &err));
		assert(err.code == UCL_SCHEMA_INVALID_SCHEMA);
		ucl_object_unref(schema);
	}

	schema = parse_msgpack(mp_type, sizeof(mp_type) - 1);
	assert(ucl_array_head(ucl_object_lookup(schema, "type"))->flags &
		   UCL_OBJECT_BINARY);
	assert(!validate(schema, NULL, "{\"v\": 1}", &err));
	assert(err.code == UCL_SCHEMA_INVALID_SCHEMA);
	ucl_object_unref(schema);

	/* Type names are still fine in any order */
	schema = parse("{\"type\": [\"string\", \"integer\"]}");
	assert(validate(schema, NULL, "{\"v\": 1}", &err));
	assert(!validate(schema, NULL, "{\"v\": true}", &err));
	assert(err.code == UCL_SCHEMA_TYPE_MISMATCH);
	ucl_object_unref(schema);
}

/*
 * A binary string cannot be passed to regexec(), so it does not satisfy
 * a pattern.
 */
static void
test_binary_string_pattern(void)
{
	/* [<bin "x">] */
	static const char mp_data[] = "\x91\xc4\x01x";
	struct ucl_schema_error err;
	ucl_object_t *schema, *data;

	schema = parse("{\"items\": {\"pattern\": \"x\"}}");
	data = parse_msgpack(mp_data, sizeof(mp_data) - 1);
	assert(ucl_array_head(data)->flags & UCL_OBJECT_BINARY);
	memset(&err, 0, sizeof(err));
	assert(!ucl_object_validate(schema, data, &err));
	assert(err.code == UCL_SCHEMA_CONSTRAINT);
	ucl_object_unref(data);
	ucl_object_unref(schema);
}

/*
 * Binary strings have no C string form either, but "enum" and "uniqueItems"
 * compare them by their bytes, on both the schema and the data side.
 */
static void
test_binary_string_compare(void)
{
	/* {"enum": [<bin "x">]} */
	static const char mp_enum[] = "\x81\xa4" "enum\x91\xc4\x01x";
	/* [<bin "x">, <bin "y">] */
	static const char mp_xy[] = "\x92\xc4\x01x\xc4\x01y";
	/* [<bin "x">, <bin "x">] */
	static const char mp_xx[] = "\x92\xc4\x01x\xc4\x01x";
	struct ucl_schema_error err;
	ucl_object_t *schema, *data;

	schema = parse_msgpack(mp_enum, sizeof(mp_enum) - 1);
	assert(ucl_array_head(ucl_object_lookup(schema, "enum"))->flags &
		   UCL_OBJECT_BINARY);
	assert(validate(schema, NULL, "{\"v\": \"x\"}", &err));
	assert(!validate(schema, NULL, "{\"v\": \"y\"}", &err));
	assert(err.code == UCL_SCHEMA_CONSTRAINT);
	ucl_object_unref(schema);

	schema = parse("{\"items\": {\"enum\": [\"x\"]}}");
	data = parse_msgpack(mp_xx, sizeof(mp_xx) - 1);
	assert(ucl_array_head(data)->flags & UCL_OBJECT_BINARY);
	memset(&err, 0, sizeof(err));
	assert(ucl_object_validate(schema, data, &err));
	ucl_object_unref(data);
	data = parse_msgpack(mp_xy, sizeof(mp_xy) - 1);
	memset(&err, 0, sizeof(err));
	assert(!ucl_object_validate(schema, data, &err));
	assert(err.code == UCL_SCHEMA_CONSTRAINT);
	ucl_object_unref(data);
	ucl_object_unref(schema);

	schema = parse("{\"uniqueItems\": true}");
	data = parse_msgpack(mp_xy, sizeof(mp_xy) - 1);
	memset(&err, 0, sizeof(err));
	assert(ucl_object_validate(schema, data, &err));
	ucl_object_unref(data);
	data = parse_msgpack(mp_xx, sizeof(mp_xx) - 1);
	memset(&err, 0, sizeof(err));
	assert(!ucl_object_validate(schema, data, &err));
	assert(err.code == UCL_SCHEMA_CONSTRAINT);
	ucl_object_unref(data);
	ucl_object_unref(schema);
}

/*
 * NaN is neither less nor greater than any number, which made it equal to
 * every number for "enum" and "uniqueItems"
 */
static void
test_nan_compare(void)
{
	struct ucl_schema_error err;
	ucl_object_t *schema, *nan_obj, *num;

	nan_obj = ucl_object_fromdouble(NAN);
	num = ucl_object_fromdouble(1.5);
	assert(ucl_object_compare(nan_obj, num) > 0);
	assert(ucl_object_compare(num, nan_obj) < 0);
	assert(ucl_object_compare(nan_obj, nan_obj) == 0);
	ucl_object_unref(num);
	ucl_object_unref(nan_obj);

	schema = parse("{\"enum\": [1.5]}");
	assert(validate(schema, NULL, "{\"v\": 1.5}", &err));
	assert(!validate(schema, NULL, "{\"v\": nan}", &err));
	assert(err.code == UCL_SCHEMA_CONSTRAINT);
	ucl_object_unref(schema);

	schema = parse("{\"enum\": [nan]}");
	assert(validate(schema, NULL, "{\"v\": nan}", &err));
	assert(!validate(schema, NULL, "{\"v\": 1.5}", &err));
	ucl_object_unref(schema);

	schema = parse("{\"uniqueItems\": true}");
	assert(validate(schema, NULL, "{\"v\": [1.5, nan, 2.5]}", &err));
	assert(!validate(schema, NULL, "{\"v\": [nan, 1.5, nan]}", &err));
	assert(err.code == UCL_SCHEMA_CONSTRAINT);
	ucl_object_unref(schema);
}

#ifndef _WIN32
/*
 * A $ref to a file or URL is fetched and parsed, and everything this
 * allocates is released whether or not that works. A file is mapped
 * rather than allocated, so it must not be passed to free().
 */
static void
test_external_ref(void)
{
	static const char content[] = "{\"type\": \"integer\"}";
	struct ucl_schema_error err;
	ucl_object_t *schema;
	char fname[] = "schema_ref_XXXXXX", buf[128];
	int fd;

	fd = mkstemp(fname);
	assert(fd != -1);
	assert(write(fd, content, sizeof(content) - 1) == sizeof(content) - 1);
	close(fd);

	snprintf(buf, sizeof(buf), "{\"$ref\": \"%s\"}", fname);
	schema = parse(buf);
	assert(validate(schema, NULL, "{\"v\": 1}", &err));
	assert(!validate(schema, NULL, "{\"v\": \"x\"}", &err));
	assert(err.code == UCL_SCHEMA_TYPE_MISMATCH);
	ucl_object_unref(schema);

	snprintf(buf, sizeof(buf), "{\"$ref\": \"%s#\"}", fname);
	schema = parse(buf);
	assert(validate(schema, NULL, "{\"v\": 1}", &err));
	ucl_object_unref(schema);

	/* A file that does not parse */
	fd = open(fname, O_WRONLY | O_TRUNC);
	assert(fd != -1);
	assert(write(fd, "{", 1) == 1);
	close(fd);
	schema = parse(buf);
	assert(!validate(schema, NULL, "{\"v\": 1}", &err));
	assert(err.code == UCL_SCHEMA_INVALID_SCHEMA);
	assert(strstr(err.msg, "cannot fetch reference") != NULL);
	ucl_object_unref(schema);
	unlink(fname);

	/* A file that does not exist */
	schema = parse(buf);
	assert(!validate(schema, NULL, "{\"v\": 1}", &err));
	assert(err.code == UCL_SCHEMA_INVALID_SCHEMA);
	assert(strstr(err.msg, "cannot fetch reference") != NULL);
	ucl_object_unref(schema);

	/* A URL that cannot be fetched, or no URL support at all */
	schema = parse("{\"$ref\": \"http://127.0.0.1:1/schema\"}");
	assert(!validate(schema, NULL, "{\"v\": 1}", &err));
	assert(err.code == UCL_SCHEMA_INVALID_SCHEMA);
	assert(strstr(err.msg, "cannot fetch reference") != NULL);
	ucl_object_unref(schema);
}
#endif

/*
 * Lengths are unsigned 32 bit, so subtracting them gave the wrong sign once
 * they were 2^31 or more apart. The lengths are faked: the contents are only
 * read when they are equal.
 */
static void
test_len_compare(void)
{
	ucl_object_t *objs[2][3];
	unsigned i, j;

	for (i = 0; i < 2; i++) {
		objs[i][0] = ucl_object_fromstring("x");
		objs[i][1] = ucl_object_typed_new(UCL_ARRAY);
		objs[i][2] = ucl_object_typed_new(UCL_OBJECT);
	}

	for (j = 0; j < 3; j++) {
		uint32_t short_len = objs[0][j]->len, long_len = objs[1][j]->len;

		objs[0][j]->len = 1;
		objs[1][j]->len = 0x80000002U;
		assert(ucl_object_compare(objs[0][j], objs[1][j]) < 0);
		assert(ucl_object_compare(objs[1][j], objs[0][j]) > 0);
		objs[0][j]->len = short_len;
		objs[1][j]->len = long_len;
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 3; j++) {
			ucl_object_unref(objs[i][j]);
		}
	}
}

/*
 * anyOf, oneOf and not took an invalid subschema for one the object does not
 * match, and reported a failure of their own with no error
 */
static void
test_combinator_errors(void)
{
	static const struct {
		const char *schema;
		enum ucl_schema_error_code code;
	} cases[] = {
		{"{\"anyOf\": [{\"$ref\": \"#/missing\"}, {\"type\": \"integer\"}]}",
		 UCL_SCHEMA_INVALID_SCHEMA},
		{"{\"oneOf\": [{\"$ref\": \"#/missing\"}, {\"type\": \"integer\"}]}",
		 UCL_SCHEMA_INVALID_SCHEMA},
		{"{\"oneOf\": [{\"type\": \"integer\"}, {\"$ref\": \"#/missing\"}]}",
		 UCL_SCHEMA_INVALID_SCHEMA},
		{"{\"not\": {\"$ref\": \"#/missing\"}}", UCL_SCHEMA_INVALID_SCHEMA},
		{"{\"not\": {\"anyOf\": []}}", UCL_SCHEMA_INVALID_SCHEMA},
		{"{\"oneOf\": [{\"type\": \"integer\"}, {\"minimum\": 0}]}",
		 UCL_SCHEMA_CONSTRAINT},
		{"{\"not\": {\"type\": \"integer\"}}", UCL_SCHEMA_CONSTRAINT},
	};
	struct ucl_schema_error err;
	ucl_object_t *schema, *doc;
	unsigned int i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		schema = parse(cases[i].schema);
		assert(!validate(schema, NULL, "{\"v\": 1}", &err));
		assert(err.code == cases[i].code);
		assert(err.msg[0] != '\0');
		doc = parse("{\"v\": 1}");
		assert(!ucl_object_validate(schema, ucl_object_lookup(doc, "v"), NULL));
		ucl_object_unref(doc);
		ucl_object_unref(schema);
	}

	/* No error is left from the subschemas that have not matched */
	schema = parse("{\"oneOf\": [{\"type\": \"string\"}, {\"type\": \"integer\"}]}");
	assert(validate(schema, NULL, "{\"v\": 1}", &err));
	assert(err.code == UCL_SCHEMA_OK);
	ucl_object_unref(schema);
	schema = parse("{\"anyOf\": [{\"type\": \"string\"}, {\"type\": \"integer\"}], "
				   "\"not\": {\"type\": \"string\"}}");
	assert(validate(schema, NULL, "{\"v\": 1}", &err));
	assert(err.code == UCL_SCHEMA_OK);
	assert(!validate(schema, NULL, "{\"v\": true}", &err));
	assert(err.code == UCL_SCHEMA_TYPE_MISMATCH);
	ucl_object_unref(schema);
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	test_shared_subschema();
	test_cross_root_cycle();
	test_non_string_ref();
	test_malformed_keywords();
	test_malformed_type_array();
	test_binary_string_pattern();
	test_binary_string_compare();
	test_nan_compare();
	test_len_compare();
	test_combinator_errors();
#ifndef _WIN32
	test_external_ref();
#endif

	printf("All schema $ref tests passed\n");

	return 0;
}
