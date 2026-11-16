#include "ucl.h"
#include "ucl_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct handler_state {
	bool bad_name;
};

static bool
expand_variable(const unsigned char *name, size_t len,
				unsigned char **replacement, size_t *replacement_len,
				bool *need_free, void *ud)
{
	struct handler_state *state = ud;
	const char *value;

	/* Both passes must receive only the name, without braces or a suffix. */
	if (memchr(name, '}', len) != NULL || memchr(name, '$', len) != NULL) {
		state->bad_name = true;
		return false;
	}

	*need_free = false;
	if (len == 1 && name[0] == 'X') {
		value = "AB";
	}
	else if (len == 1 && name[0] == 'Y') {
		value = "C";
	}
	else if (len == 5 && memcmp(name, "EMPTY", len) == 0) {
		value = "";
	}
	else if (len == 4 && memcmp(name, "LONG", len) == 0) {
		value = "a longer replacement";
		*need_free = true;
	}
	else if (len == 6 && memcmp(name, "BINARY", len) == 0) {
		*replacement = (unsigned char *) "A\0B";
		*replacement_len = 3;
		return true;
	}
	else {
		return false;
	}

	*replacement_len = strlen(value);
	if (*need_free) {
		*replacement = malloc(*replacement_len + 1);
		if (*replacement == NULL) {
			return false;
		}
		memcpy(*replacement, value, *replacement_len + 1);
	}
	else {
		*replacement = (unsigned char *) value;
	}
	return true;
}

static bool
check_expansion(const char *input, const char *expected, size_t expected_len)
{
	struct handler_state state = {false};
	struct ucl_parser *parser = ucl_parser_new(0);
	ucl_object_t *obj = NULL;
	const ucl_object_t *value;
	const char *actual;
	size_t actual_len;
	char config[256];
	bool ok = false;

	if (parser == NULL) {
		return false;
	}

	ucl_parser_register_variable(parser, "BUILTIN", "registered");
	ucl_parser_set_variables_handler(parser, expand_variable, &state);
	snprintf(config, sizeof(config), "value = \"%s\";", input);
	if (!ucl_parser_add_string(parser, config, strlen(config))) {
		fprintf(stderr, "%s: parse failed: %s\n", input,
				ucl_parser_get_error(parser));
		goto done;
	}

	obj = ucl_parser_get_object(parser);
	value = ucl_object_lookup(obj, "value");
	actual = ucl_object_tolstring(value, &actual_len);
	if (state.bad_name || actual == NULL || actual_len != expected_len ||
		memcmp(actual, expected, expected_len) != 0) {
		fprintf(stderr, "%s: incorrect expansion or callback name\n", input);
		goto done;
	}
	ok = true;

done:
	ucl_object_unref(obj);
	ucl_parser_free(parser);
	return ok;
}

static struct ucl_variable *
find_variable(struct ucl_parser *parser, const char *name, int *count)
{
	struct ucl_variable *var, *found = NULL;

	*count = 0;
	for (var = parser->variables; var != NULL; var = var->next) {
		if (strcmp(var->var, name) == 0) {
			found = var;
			(*count)++;
		}
	}

	return found;
}

/*
 * While an included file is parsed, its own FILENAME, CURDIR and current file
 * replace those of the including document. A failed include must hand them
 * back just like a successful one: they used to stay detached from the parser
 * and leak, with the included file left as the current one.
 */
static bool
check_failed_include(const char *base, const char *args, const char *included,
					 const char *top_file)
{
	const char *dir = getenv("TEST_OUT_DIR");
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_variable *filename, *curdir, *var;
	char path[1024], config[2048];
	const char *cur_file;
	FILE *out;
	int count;
	bool ok = false;

	if (parser == NULL) {
		return false;
	}

	snprintf(path, sizeof(path), "%s/variables_failed.inc",
			 dir != NULL ? dir : ".");
	out = fopen(path, "w");
	if (out == NULL) {
		fprintf(stderr, "cannot create %s\n", path);
		goto done;
	}
	fputs(included, out);
	if (fclose(out) != 0) {
		fprintf(stderr, "cannot write %s\n", path);
		goto done;
	}

	if (top_file != NULL) {
		ucl_parser_set_filevars(parser, top_file, false);
	}
	filename = find_variable(parser, "FILENAME", &count);
	curdir = find_variable(parser, "CURDIR", &count);
	snprintf(config, sizeof(config), "%s\n.include%s \"%s\"\n", base, args, path);

	if (ucl_parser_add_string(parser, config, 0)) {
		fprintf(stderr, "%s: the include must fail\n", included);
		goto done;
	}

	ok = true;
	cur_file = ucl_parser_get_cur_file(parser);
	if (top_file != NULL ? (cur_file == NULL || strcmp(cur_file, top_file) != 0)
						 : cur_file != NULL) {
		fprintf(stderr, "%s: current file is %s after a failed include\n",
				included, cur_file != NULL ? cur_file : "(null)");
		ok = false;
	}
	var = find_variable(parser, "FILENAME", &count);
	if (var != filename || count != 1) {
		fprintf(stderr, "%s: FILENAME was not restored after a failed include\n",
				included);
		ok = false;
	}
	var = find_variable(parser, "CURDIR", &count);
	if (var != curdir || count != 1) {
		fprintf(stderr, "%s: CURDIR was not restored after a failed include\n",
				included);
		ok = false;
	}

done:
	remove(path);
	ucl_parser_free(parser);
	return ok;
}

/* The value of a variable, or NULL unless it is registered exactly once */
static const char *
registered_variable(struct ucl_parser *parser, const char *name)
{
	struct ucl_variable *var;
	const char *value = NULL;
	int found = 0;

	for (var = parser->variables; var != NULL; var = var->next) {
		if (strcmp(var->var, name) == 0) {
			value = var->value;
			found++;
		}
	}

	return found == 1 ? value : NULL;
}

static void
test_path(char *buf, size_t len, const char *name)
{
	const char *dir = getenv("TEST_OUT_DIR");

	snprintf(buf, len, "%s/%s", dir != NULL ? dir : ".", name);
}

static bool
write_test_file(const char *name, const char *content)
{
	char path[1024];
	FILE *out;

	test_path(path, sizeof(path), name);
	out = fopen(path, "w");
	if (out == NULL) {
		return false;
	}
	fputs(content, out);
	return fclose(out) == 0;
}

static bool
same_string(const char *a, const char *b)
{
	return a != NULL && b != NULL && strcmp(a, b) == 0;
}

/*
 * An include replaces FILENAME, CURDIR and the current file with the ones of
 * the included file while it is parsed. They must belong to the including
 * file again afterwards, whether the include succeeded or not. A config that
 * parses expands them into "file" and "dir" after its include.
 */
static bool
check_include_filevars(const char *config, bool expect_ok)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	ucl_object_t *obj = NULL;
	char path[1024], *cur_file = NULL, *filename = NULL, *curdir = NULL;
	bool ok = false;

	test_path(path, sizeof(path), "variables_parent.conf");
	if (parser == NULL || !write_test_file("variables_parent.conf", config) ||
		!ucl_parser_set_filevars(parser, path, true)) {
		fprintf(stderr, "%s: cannot set up the including file\n", config);
		goto done;
	}

	/* What the including file sees; ucl_parser_add_file() sets the same */
	if (ucl_parser_get_cur_file(parser) != NULL) {
		cur_file = strdup(ucl_parser_get_cur_file(parser));
	}
	if (registered_variable(parser, "FILENAME") != NULL) {
		filename = strdup(registered_variable(parser, "FILENAME"));
	}
	if (registered_variable(parser, "CURDIR") != NULL) {
		curdir = strdup(registered_variable(parser, "CURDIR"));
	}
	if (cur_file == NULL || filename == NULL || curdir == NULL) {
		fprintf(stderr, "%s: file variables are not set\n", config);
		goto done;
	}

	if (ucl_parser_add_file(parser, path) != expect_ok) {
		fprintf(stderr, "%s: parse %s: %s\n", config,
				expect_ok ? "failed" : "succeeded",
				ucl_parser_get_error(parser) ? ucl_parser_get_error(parser) : "");
		goto done;
	}
	ok = true;
	if (!same_string(ucl_parser_get_cur_file(parser), cur_file)) {
		fprintf(stderr, "%s: current file is not restored\n", config);
		ok = false;
	}
	if (!same_string(registered_variable(parser, "FILENAME"), filename) ||
		!same_string(registered_variable(parser, "CURDIR"), curdir)) {
		fprintf(stderr, "%s: FILENAME or CURDIR is not restored\n", config);
		ok = false;
	}
	if (expect_ok) {
		obj = ucl_parser_get_object(parser);
		if (!same_string(ucl_object_tostring(ucl_object_lookup(obj, "file")),
						 filename) ||
			!same_string(ucl_object_tostring(ucl_object_lookup(obj, "dir")),
						 curdir)) {
			fprintf(stderr, "%s: wrong FILENAME or CURDIR after the include\n",
					config);
			ok = false;
		}
	}

done:
	ucl_object_unref(obj);
	free(curdir);
	free(filename);
	free(cur_file);
	remove(path);
	ucl_parser_free(parser);
	return ok;
}

int
main(void)
{
	static const struct {
		const char *input;
		const char *expected;
		size_t expected_len;
	} cases[] = {
#define CASE(input, expected) {input, expected, sizeof(expected) - 1}
		CASE("${X}", "AB"),
		CASE("pre${X}", "preAB"),
		CASE("${X}post", "ABpost"),
		CASE("pre${X}post", "preABpost"),
		CASE("${X}${Y}", "ABC"),
		CASE("pre${X}mid${X}post", "preABmidABpost"),
		CASE("${BUILTIN}/${X}/${Y}", "registered/AB/C"),
		CASE("$BUILTIN/${X}", "registered/AB"),
		CASE("${MISSING}-${X}-${MISSING}", "${MISSING}-AB-${MISSING}"),
		CASE("$${X}-${Y}-$$", "${X}-C-$"),
		CASE("${X}-${MISSING", "AB-${MISSING"),
		CASE("${X}-${", "AB-${"),
		CASE("${EMPTY}", ""),
		CASE("pre${EMPTY}post", "prepost"),
		CASE("${EMPTY}${X}${EMPTY}${Y}", "ABC"),
		CASE("pre${LONG}post", "prea longer replacementpost"),
		CASE("pre${BINARY}post", "preA\0Bpost"),
#undef CASE
	};
	static const struct {
		const char *config;
		bool expect_ok;
	} includes[] = {
		{"a = 1; .include \"${CURDIR}/variables_good.inc\"; "
		 "file = \"${FILENAME}\"; dir = \"${CURDIR}\"",
		 true},
		/* The include fails, yet a glob with try goes on with the parse */
		{"a = 1; .try_include(glob=true, key=\"a\") \"${CURDIR}/variables_goo*.inc\"; "
		 "file = \"${FILENAME}\"; dir = \"${CURDIR}\"",
		 true},
		{"a = 1; .include \"${CURDIR}/variables_bad.inc\"", false},
		/* Each level of a nested include has to restore its parent's */
		{"a = 1; .include \"${CURDIR}/variables_nested.inc\"", false},
		{"a = 1; .include(glob=true) \"${CURDIR}/variables_ba*.inc\"", false},
		/* Fails before the included file is parsed */
		{"a = 1; .include(key=\"a\") \"${CURDIR}/variables_good.inc\"", false},
	};
	static const struct {
		const char *name;
		const char *content;
	} include_files[] = {
		{"variables_good.inc", "b = 2;\n"},
		{"variables_bad.inc", "}}}\n"},
		{"variables_nested.inc",
		 "c = 3;\n.include \"${CURDIR}/variables_bad.inc\"\n"},
	};
	char path[1024];
	bool ok = true, created = true;

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		if (!check_expansion(cases[i].input, cases[i].expected,
							 cases[i].expected_len)) {
			ok = false;
		}
	}
	/* The included file does not parse */
	ok = check_failed_include("", "", "a = {{;", NULL) && ok;
	ok = check_failed_include("", "", "a = {{;", "/nonexistent/top.conf") && ok;
	/* Rejected before parsing: the prefix names a key that is not an object */
	ok = check_failed_include("key = 1;", "(key=\"key\")", "a = 1;",
							  "/nonexistent/top.conf") && ok;

	for (size_t i = 0; i < sizeof(include_files) / sizeof(include_files[0]); i++) {
		if (!write_test_file(include_files[i].name, include_files[i].content)) {
			created = false;
		}
	}
	if (!created) {
		fprintf(stderr, "cannot create the included files\n");
		ok = false;
	}
	else {
		for (size_t i = 0; i < sizeof(includes) / sizeof(includes[0]); i++) {
			if (!check_include_filevars(includes[i].config,
										includes[i].expect_ok)) {
				ok = false;
			}
		}
	}
	for (size_t i = 0; i < sizeof(include_files) / sizeof(include_files[0]); i++) {
		test_path(path, sizeof(path), include_files[i].name);
		remove(path);
	}

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
