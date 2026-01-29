/*
 * test-runner.c
 * test harness
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

#include <libpkgconf/libpkgconf.h>
#include <libpkgconf/stdinc.h>
#include <cli/core.h>
#include <cli/getopt_long.h>

#if !defined(_WIN32) && !defined(__HAIKU__)
# define PKGCONF_TEST_PLATFORM "unix"
#elif !defined(_WIN32)
# define PKGCONF_TEST_PLATFORM "haiku"
#else
# define PKGCONF_TEST_PLATFORM "windows"
#endif

static char *test_fixtures_dir = NULL;
static bool debug = false;

typedef enum test_match_strategy_ {
	MATCH_EXACT = 0,
	MATCH_PARTIAL,
	MATCH_EMPTY,
} pkgconf_test_match_strategy_t;

typedef struct test_bufferset_ {
	pkgconf_node_t node;
	pkgconf_buffer_t buffer;
} pkgconf_test_bufferset_t;

typedef struct test_case_ {
	char *name;

	pkgconf_list_t search_path;
	pkgconf_buffer_t query;

	pkgconf_list_t expected_stdout;
	pkgconf_test_match_strategy_t match_stdout;

	pkgconf_list_t expected_stderr;
	pkgconf_test_match_strategy_t match_stderr;

	int exitcode;
	uint64_t wanted_flags;

	pkgconf_list_t env_vars;

	pkgconf_buffer_t want_env_prefix;
	pkgconf_buffer_t want_variable;
	pkgconf_buffer_t fragment_filter;

	pkgconf_buffer_t skip_platforms;

	pkgconf_list_t define_variables;

	int verbosity;

	pkgconf_buffer_t atleast_version;
	pkgconf_buffer_t exact_version;
	pkgconf_buffer_t max_version;
} pkgconf_test_case_t;

typedef struct test_state_ {
	pkgconf_cli_state_t cli_state;
	const pkgconf_test_case_t *testcase;
} pkgconf_test_state_t;

typedef struct test_environ_ {
	pkgconf_node_t node;
	char *key;
	char *value;
} pkgconf_test_environ_t;

typedef struct test_output_ {
	pkgconf_output_t output;

	pkgconf_buffer_t o_stdout;
	pkgconf_buffer_t o_stderr;
} pkgconf_test_output_t;

pkgconf_test_bufferset_t *
test_bufferset_extend(pkgconf_list_t *list, pkgconf_buffer_t *buffer)
{
	pkgconf_test_bufferset_t *set = calloc(1, sizeof(*set));

	if (pkgconf_buffer_len(buffer))
		pkgconf_buffer_append(&set->buffer, pkgconf_buffer_str(buffer));

	pkgconf_node_insert_tail(&set->node, set, list);

	return set;
}

void
test_bufferset_free(pkgconf_list_t *list)
{
	pkgconf_node_t *n, *tn;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(list->head, tn, n)
	{
		pkgconf_test_bufferset_t *set = n->data;

		pkgconf_buffer_finalize(&set->buffer);
		pkgconf_node_delete(&set->node, list);

		free(set);
	}
}

void
test_environment_push(pkgconf_test_case_t *testcase, const char *key, const char *value)
{
	pkgconf_test_environ_t *env = calloc(1, sizeof(*env));
	if (env == NULL)
		return;

	env->key = strdup(key);
	env->value = strdup(value);
	pkgconf_node_insert_tail(&env->node, env, &testcase->env_vars);
}

void
test_environment_free(pkgconf_list_t *env_list)
{
	pkgconf_node_t *n, *tn;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(env_list->head, tn, n)
	{
		pkgconf_test_environ_t *env = n->data;

		pkgconf_node_delete(&env->node, env_list);

		free(env->key);
		free(env->value);
		free(env);
	}
}

static const char *
environ_lookup_handler(const pkgconf_client_t *client, const char *key)
{
	pkgconf_test_state_t *state = client->client_data;
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(state->testcase->env_vars.head, node)
	{
		pkgconf_test_environ_t *env = node->data;

		if (!strcmp(key, env->key))
			return env->value;
	}

	return NULL;
}

static bool
debug_handler(const char *msg, const pkgconf_client_t *client, void *data)
{
	(void) client;
	(void) data;
	fprintf(stderr, "%s", msg);
	return true;
}

static bool
error_handler(const char *msg, const pkgconf_client_t *client, void *data)
{
	(void) data;
	pkgconf_test_state_t *state = client->client_data;
	pkgconf_output_fmt(client->output, state->testcase->wanted_flags & PKG_ERRORS_ON_STDOUT ? PKGCONF_OUTPUT_STDOUT : PKGCONF_OUTPUT_STDERR, "%s", msg);
	return true;
}

static bool
write_handler(pkgconf_output_t *output, pkgconf_output_stream_t stream, const pkgconf_buffer_t *buffer)
{
	pkgconf_test_output_t *out = (pkgconf_test_output_t *) output;
	pkgconf_buffer_t *dest = stream == PKGCONF_OUTPUT_STDERR ? &out->o_stderr : &out->o_stdout;

	pkgconf_buffer_append(dest, pkgconf_buffer_str(buffer));
	return true;
}

static pkgconf_output_t *
test_output(void)
{
	static pkgconf_test_output_t output = {
		.output.write = write_handler,
	};

	return &output.output;
}

static void
test_output_reset(pkgconf_test_output_t *out)
{
	pkgconf_buffer_reset(&out->o_stdout);
	pkgconf_buffer_reset(&out->o_stderr);
}

static void
handle_substs(pkgconf_buffer_t *dest, const pkgconf_buffer_t *src)
{
	const struct subst_pair {
		char *key;
		char *value;
	} subst_pairs[] = {
		{"%TEST_FIXTURES_DIR%", test_fixtures_dir},
		{"%DIR_SEP%", PKG_CONFIG_PATH_SEP_S},
	};

	pkgconf_buffer_t workbuf_src = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_buffer_t workbuf_dest = PKGCONF_BUFFER_INITIALIZER;

	if (!pkgconf_buffer_len(src))
		return;

	pkgconf_buffer_append(&workbuf_dest, pkgconf_buffer_str(src));

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(subst_pairs); i++)
	{
		pkgconf_buffer_reset(&workbuf_src);
		pkgconf_buffer_append(&workbuf_src, pkgconf_buffer_str(&workbuf_dest));

		pkgconf_buffer_reset(&workbuf_dest);
		pkgconf_buffer_subst(&workbuf_dest, &workbuf_src, subst_pairs[i].key, subst_pairs[i].value);
	}

	pkgconf_buffer_append(dest, pkgconf_buffer_str(&workbuf_dest));

	pkgconf_buffer_finalize(&workbuf_src);
	pkgconf_buffer_finalize(&workbuf_dest);
}

typedef void (*test_keyword_func_t)(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, char *value);

typedef struct test_keyword_pair_ {
	const char *keyword;
	const test_keyword_func_t func;
	const ptrdiff_t offset;
} pkgconf_test_keyword_pair_t;

static int
test_keyword_pair_cmp(const void *key, const void *ptr)
{
	const pkgconf_test_keyword_pair_t *pair = ptr;
	return strcasecmp(key, pair->keyword);
}

static void
test_keyword_set_int(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, char *value)
{
	(void) keyword;
	(void) warnprefix;

	int *dest = (int *)((char *) testcase + offset);
	*dest = atoi(value);
}

static void
test_keyword_set_buffer(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, char *value)
{
	(void) keyword;
	(void) warnprefix;

	pkgconf_buffer_t *dest = (pkgconf_buffer_t *)((char *) testcase + offset);
	handle_substs(dest, PKGCONF_BUFFER_FROM_STR(value));
}

static void
test_keyword_extend_bufferset(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, char *value)
{
	(void) keyword;
	(void) warnprefix;

	pkgconf_list_t *dest = (pkgconf_list_t *)((char *) testcase + offset);
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	handle_substs(&buf, PKGCONF_BUFFER_FROM_STR(value));
	test_bufferset_extend(dest, &buf);
	pkgconf_buffer_finalize(&buf);
}

typedef struct test_flag_pair_ {
	const char *name;
	uint64_t flag;
} pkgconf_test_flag_pair_t;

static int
test_flag_pair_cmp(const void *key, const void *ptr)
{
	const pkgconf_test_flag_pair_t *pair = ptr;
	return strcasecmp(key, pair->name);
}

static const pkgconf_test_flag_pair_t test_flag_pairs[] = {
	{"cflags",		PKG_CFLAGS},
	{"cflags-only-i",	PKG_CFLAGS_ONLY_I},
	{"cflags-only-other",	PKG_CFLAGS_ONLY_OTHER},
	{"debug",		PKG_DEBUG},
	{"define-prefix",	PKG_DEFINE_PREFIX},
	{"digraph",		PKG_DIGRAPH},
	{"dont-define-prefix",	PKG_DONT_DEFINE_PREFIX},
	{"dont-relocate-paths",	PKG_DONT_RELOCATE_PATHS},
	{"dump-license",	PKG_DUMP_LICENSE},
	{"dump-license-file",	PKG_DUMP_LICENSE_FILE},
	{"dump-personality",	PKG_DUMP_PERSONALITY},
	{"dump-source",		PKG_DUMP_SOURCE},
	{"env-only",		PKG_ENV_ONLY},
	{"errors-on-stdout",	PKG_ERRORS_ON_STDOUT},
	{"exists",		PKG_EXISTS},
	{"exists-cflags",	PKG_EXISTS_CFLAGS},
	{"fragment-tree",	PKG_FRAGMENT_TREE},
	{"ignore-conflicts",	PKG_IGNORE_CONFLICTS},
	{"internal-cflags",	PKG_INTERNAL_CFLAGS},
	{"keep-system-cflags",	PKG_KEEP_SYSTEM_CFLAGS},
	{"keep-system-libs",	PKG_KEEP_SYSTEM_LIBS},
	{"libs",		PKG_LIBS},
	{"libs-only-ldpath",	PKG_LIBS_ONLY_LDPATH},
	{"libs-only-libname",	PKG_LIBS_ONLY_LIBNAME},
	{"libs-only-other",	PKG_LIBS_ONLY_OTHER},
	{"list",		PKG_LIST},
	{"list-package-names",	PKG_LIST_PACKAGE_NAMES},
	{"modversion",		PKG_MODVERSION},
	{"msvc-syntax",		PKG_MSVC_SYNTAX},
	{"newlines",		PKG_NEWLINES},
	{"no-cache",		PKG_NO_CACHE},
	{"no-provides",		PKG_NO_PROVIDES},
	{"no-uninstalled",	PKG_NO_UNINSTALLED},
	{"path",		PKG_PATH},
	{"print-errors",	PKG_PRINT_ERRORS},
	{"print-provides",		PKG_PROVIDES},
	{"print-requires",		PKG_REQUIRES},
	{"print-requires-private",	PKG_REQUIRES_PRIVATE},
	{"print-variables",		PKG_VARIABLES},
	{"pure",		PKG_PURE},
	{"shared",		PKG_SHARED},
	{"short-errors",	PKG_SHORT_ERRORS},
	{"silence-errors",	PKG_SILENCE_ERRORS},
	{"simulate",		PKG_SIMULATE},
	{"solution",		PKG_SOLUTION},
	{"static",		PKG_STATIC},
	{"uninstalled",		PKG_UNINSTALLED},
	{"validate",		PKG_VALIDATE},
};

static void
test_keyword_set_wanted_flags(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, char *value)
{
	int i;
	int flagcount;
	char **flags = NULL;

	(void) keyword;
	(void) warnprefix;
	(void) offset;

	pkgconf_argv_split(value, &flagcount, &flags);

	for (i = 0; i < flagcount; i++)
	{
		const char *flag = flags[i];
		const pkgconf_test_flag_pair_t *pair = bsearch(flag,
			test_flag_pairs, PKGCONF_ARRAY_SIZE(test_flag_pairs),
			sizeof(*pair), test_flag_pair_cmp);

		if (pair == NULL)
			continue;

		testcase->wanted_flags |= pair->flag;
	}

	pkgconf_argv_free(flags);
}

static size_t
prefixed_path_split(const char *text, pkgconf_list_t *dirlist, const char *prefix)
{
	size_t count = 0;
	char *workbuf, *p, *iter;

	if (text == NULL)
		return 0;

	iter = workbuf = strdup(text);
	while ((p = strtok(iter, PKG_CONFIG_PATH_SEP_S)) != NULL)
	{
		pkgconf_buffer_t pathbuf = PKGCONF_BUFFER_INITIALIZER;

		pkgconf_buffer_append(&pathbuf, prefix);
		pkgconf_buffer_append(&pathbuf, "/");
		pkgconf_buffer_append(&pathbuf, p);
		pkgconf_path_add(pkgconf_buffer_str(&pathbuf), dirlist, false);
		pkgconf_buffer_finalize(&pathbuf);

		count++, iter = NULL;
	}
	free(workbuf);

	return count;
}

static void
test_keyword_set_path_list(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, char *value)
{
	(void) keyword;
	(void) warnprefix;

	pkgconf_list_t *dest = (pkgconf_list_t *)((char *) testcase + offset);
	prefixed_path_split(value, dest, test_fixtures_dir);
}

static void
test_keyword_set_match_strategy(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, char *value)
{
	(void) keyword;
	(void) warnprefix;

	pkgconf_test_match_strategy_t *dest = (pkgconf_test_match_strategy_t *)((char *) testcase + offset);

	if (!strcasecmp(value, "partial"))
		*dest = MATCH_PARTIAL;

	if (!strcasecmp(value, "empty"))
		*dest = MATCH_EMPTY;
}

static void
test_keyword_set_environment(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, char *value)
{
	(void) keyword;
	(void) offset;

	char *eq = strchr(value, '=');
	if (eq == NULL)
	{
		fprintf(stderr, "%s: malformed Environment entry: %s\n", warnprefix, value);
		return;
	}

	*eq++ = '\0';

	pkgconf_buffer_t env_value = PKGCONF_BUFFER_INITIALIZER;
	handle_substs(&env_value, PKGCONF_BUFFER_FROM_STR(eq));

	test_environment_push(testcase, value, pkgconf_buffer_str(&env_value));
	pkgconf_buffer_finalize(&env_value);
}

static const pkgconf_test_keyword_pair_t test_keyword_pairs[] = {
	{"AtLeastVersion", test_keyword_set_buffer, offsetof(pkgconf_test_case_t, atleast_version)},
	{"DefineVariable", test_keyword_extend_bufferset, offsetof(pkgconf_test_case_t, define_variables)},
	{"Environment", test_keyword_set_environment, offsetof(pkgconf_test_case_t, env_vars)},
	{"ExactVersion", test_keyword_set_buffer, offsetof(pkgconf_test_case_t, exact_version)},
	{"ExpectedExitCode", test_keyword_set_int, offsetof(pkgconf_test_case_t, exitcode)},
	{"ExpectedStderr", test_keyword_extend_bufferset, offsetof(pkgconf_test_case_t, expected_stderr)},
	{"ExpectedStdout", test_keyword_extend_bufferset, offsetof(pkgconf_test_case_t, expected_stdout)},
	{"FragmentFilter", test_keyword_set_buffer, offsetof(pkgconf_test_case_t, fragment_filter)},
	{"MatchStderr", test_keyword_set_match_strategy, offsetof(pkgconf_test_case_t, match_stderr)},
	{"MatchStdout", test_keyword_set_match_strategy, offsetof(pkgconf_test_case_t, match_stdout)},
	{"MaxVersion", test_keyword_set_buffer, offsetof(pkgconf_test_case_t, max_version)},
	{"PackageSearchPath", test_keyword_set_path_list, offsetof(pkgconf_test_case_t, search_path)},
	{"Query", test_keyword_set_buffer, offsetof(pkgconf_test_case_t, query)},
	{"SkipPlatforms", test_keyword_set_buffer, offsetof(pkgconf_test_case_t, skip_platforms)},
	{"VerbosityLevel", test_keyword_set_int, offsetof(pkgconf_test_case_t, verbosity)},
	{"WantedFlags", test_keyword_set_wanted_flags, offsetof(pkgconf_test_case_t, wanted_flags)},
	{"WantEnvPrefix", test_keyword_set_buffer, offsetof(pkgconf_test_case_t, want_env_prefix)},
	{"WantVariable", test_keyword_set_buffer, offsetof(pkgconf_test_case_t, want_variable)},
};

static void
test_keyword_set(pkgconf_test_case_t *testcase, const char *warnprefix, const char *keyword, char *value)
{
	const pkgconf_test_keyword_pair_t *pair = bsearch(keyword,
		test_keyword_pairs, PKGCONF_ARRAY_SIZE(test_keyword_pairs),
		sizeof(*pair), test_keyword_pair_cmp);

	if (pair == NULL || pair->func == NULL)
		return;

	pair->func(testcase, warnprefix, keyword, pair->offset, value);
}

static const pkgconf_parser_operand_func_t test_parser_ops[256] = {
	[':'] = (pkgconf_parser_operand_func_t) test_keyword_set,
};

static void
test_parser_warn(void *p, const char *fmt, ...)
{
	va_list va;

	(void) p;

	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

pkgconf_test_case_t *
load_test_case(char *testfile)
{
	FILE *testf = fopen(testfile, "r");
	if (testf == NULL)
		return NULL;

	pkgconf_test_case_t *out = calloc(1, sizeof(*out));
	if (out == NULL)
		goto cleanup;

	char *nameptr;
	if ((nameptr = strrchr(testfile, PKG_DIR_SEP_S)) != NULL)
		nameptr++;
	else if ((nameptr = strrchr(testfile, '/')) != NULL)
		nameptr++;
	else
		nameptr = testfile;

	out->name = strdup(nameptr);
	pkgconf_parser_parse(testf, out, test_parser_ops, test_parser_warn, testfile);

cleanup:
	fclose(testf);
	return out;
}

/* we use a custom personality to ensure the tests are fully hermetic */
pkgconf_cross_personality_t *
personality_for_test(const pkgconf_test_case_t *testcase)
{
	pkgconf_cross_personality_t *pers = calloc(1, sizeof(*pers));
	if (pers == NULL)
		return NULL;

	pers->name = strdup("test");
	pkgconf_path_copy_list(&pers->dir_list, &testcase->search_path);
	pkgconf_path_add("/test/sysroot/include", &pers->filter_includedirs, false);
	pkgconf_path_add("/test/sysroot/lib", &pers->filter_libdirs, false);

	return pers;
}

static bool
report_failure(pkgconf_test_match_strategy_t match, const pkgconf_buffer_t *expected, const pkgconf_buffer_t *actual, const char *buffername)
{
	fprintf(stderr,
		"================================================================================\n"
		"%s did not%s match:\n"
		"  expected: [%s]\n"
		"  actual: [%s]\n"
		"================================================================================\n",
		buffername, match == MATCH_PARTIAL ? " partially" : "",
		pkgconf_buffer_str_or_empty(expected),
		pkgconf_buffer_str_or_empty(actual));

	return false;
}

bool
test_match_buffer(pkgconf_test_match_strategy_t match, const pkgconf_buffer_t *expected, const pkgconf_buffer_t *actual, const char *buffername)
{
	if (!pkgconf_buffer_len(expected) && match != MATCH_EMPTY)
		return true;

	if (!pkgconf_buffer_len(actual))
	{
		if (match == MATCH_EMPTY)
			return true;

		return report_failure(match, expected, actual, buffername);
	}

	if (match == MATCH_PARTIAL)
		return pkgconf_buffer_contains(actual, expected) ? true : report_failure(match, expected, actual, buffername);

	return pkgconf_buffer_match(actual, expected) ? true : report_failure(match, expected, actual, buffername);
}

void
annotate_result(const pkgconf_test_case_t *testcase, int ret, const pkgconf_test_output_t *out)
{
	pkgconf_buffer_t search_path_buf = PKGCONF_BUFFER_INITIALIZER;
	const pkgconf_node_t *n;

	PKGCONF_FOREACH_LIST_ENTRY(testcase->search_path.head, n)
	{
		const pkgconf_path_t *path = n->data;

		if (pkgconf_buffer_len(&search_path_buf))
			pkgconf_buffer_push_byte(&search_path_buf, ' ');

		pkgconf_buffer_append(&search_path_buf, path->path);
	}

	pkgconf_buffer_t wanted_flags_buf = PKGCONF_BUFFER_INITIALIZER;

	for (size_t i = 0; i < PKGCONF_ARRAY_SIZE(test_flag_pairs); i++)
	{
		const pkgconf_test_flag_pair_t *pair = &test_flag_pairs[i];

		if ((testcase->wanted_flags & pair->flag) == pair->flag)
		{
			if (pkgconf_buffer_len(&wanted_flags_buf))
				pkgconf_buffer_push_byte(&wanted_flags_buf, ' ');

			pkgconf_buffer_append(&wanted_flags_buf, pair->name);
		}
	}

	pkgconf_buffer_t env_buf = PKGCONF_BUFFER_INITIALIZER;

	PKGCONF_FOREACH_LIST_ENTRY(testcase->env_vars.head, n)
	{
		const pkgconf_test_environ_t *env = n->data;

		if (pkgconf_buffer_len(&env_buf))
			pkgconf_buffer_append(&env_buf, "\n  ");

		pkgconf_buffer_append_fmt(&env_buf, "%s: %s", env->key, env->value);
	}

	fprintf(stderr,
		"--------------------------------------------------------------------------------\n"
		"search-path: <%s>\n"
		"wanted-flags: <%s>\n"
                "environment:\n"
                "  %s\n"
		"query: [%s]\n"
		"exit-code: %d\n"
		"verbosity: %d\n",
		pkgconf_buffer_str_or_empty(&search_path_buf),
		pkgconf_buffer_str_or_empty(&wanted_flags_buf),
		pkgconf_buffer_str_or_empty(&env_buf),
		pkgconf_buffer_str_or_empty(&testcase->query),
		ret,
		testcase->verbosity);

	fprintf(stderr, "stdout: [%s]\n",
		pkgconf_buffer_str_or_empty(&out->o_stdout));

	PKGCONF_FOREACH_LIST_ENTRY(testcase->expected_stdout.head, n)
	{
		pkgconf_test_bufferset_t *set = n->data;

		fprintf(stderr,
			"expected-stdout: [%s] (%s)\n",
			pkgconf_buffer_str_or_empty(&set->buffer),
			testcase->match_stdout == MATCH_PARTIAL ? "partial" : "exact");
	}

	fprintf(stderr,	"stderr: [%s]\n",
		pkgconf_buffer_str_or_empty(&out->o_stderr));

	PKGCONF_FOREACH_LIST_ENTRY(testcase->expected_stderr.head, n)
	{
		pkgconf_test_bufferset_t *set = n->data;

		fprintf(stderr,
			"expected-stderr: [%s] (%s)\n",
			pkgconf_buffer_str_or_empty(&set->buffer),
			testcase->match_stderr == MATCH_PARTIAL ? "partial" : "exact");
	}

	PKGCONF_FOREACH_LIST_ENTRY(testcase->define_variables.head, n)
	{
		pkgconf_test_bufferset_t *set = n->data;

		fprintf(stderr,	"define-variable: [%s]\n", pkgconf_buffer_str_or_empty(&set->buffer));
	}

	fprintf(stderr,
		"want-env-prefix: [%s]\n"
		"fragment-filter: [%s]\n"
		"--------------------------------------------------------------------------------\n",
		pkgconf_buffer_str_or_empty(&testcase->want_env_prefix),
		pkgconf_buffer_str_or_empty(&testcase->fragment_filter));

	pkgconf_buffer_finalize(&search_path_buf);
	pkgconf_buffer_finalize(&wanted_flags_buf);
	pkgconf_buffer_finalize(&env_buf);
}

bool
run_test_case(const pkgconf_test_case_t *testcase)
{
	bool passed = true;

	const pkgconf_buffer_t *our_platform = PKGCONF_BUFFER_FROM_STR(PKGCONF_TEST_PLATFORM);
	if (pkgconf_buffer_contains(&testcase->skip_platforms, our_platform))
	{
		printf("# test skipped on %s\nSKIP: %s\n",
			pkgconf_buffer_str(our_platform), testcase->name);
		return true;
	}

	pkgconf_cross_personality_t *personality = personality_for_test(testcase);
	pkgconf_test_state_t state = {
		.cli_state.want_flags = testcase->wanted_flags,
		.cli_state.want_env_prefix = pkgconf_buffer_str(&testcase->want_env_prefix),
		.cli_state.want_variable = pkgconf_buffer_str(&testcase->want_variable),
		.cli_state.want_fragment_filter = pkgconf_buffer_str(&testcase->fragment_filter),
		.cli_state.required_module_version = pkgconf_buffer_str(&testcase->atleast_version),
		.cli_state.required_exact_module_version = pkgconf_buffer_str(&testcase->exact_version),
		.cli_state.required_max_module_version = pkgconf_buffer_str(&testcase->max_version),
		.cli_state.verbosity = testcase->verbosity,
		.testcase = testcase,
	};

	pkgconf_test_output_t *out = (pkgconf_test_output_t *) test_output();

	pkgconf_client_init(&state.cli_state.pkg_client, error_handler, NULL, personality, &state, environ_lookup_handler);
	pkgconf_client_set_output(&state.cli_state.pkg_client, &out->output);

	pkgconf_node_t *n;

	PKGCONF_FOREACH_LIST_ENTRY(testcase->define_variables.head, n)
	{
		pkgconf_test_bufferset_t *set = n->data;

		pkgconf_tuple_define_global(&state.cli_state.pkg_client, pkgconf_buffer_str_or_empty(&set->buffer));
	}

	pkgconf_buffer_t arg_buf = PKGCONF_BUFFER_INITIALIZER;
	int test_argc = 0;
	char **test_argv = NULL;

	pkgconf_buffer_append_fmt(&arg_buf, "pkgconf %s", pkgconf_buffer_str_or_empty(&testcase->query));
	pkgconf_argv_split(pkgconf_buffer_str(&arg_buf), &test_argc, &test_argv);
	pkgconf_buffer_finalize(&arg_buf);

	pkgconf_client_set_warn_handler(&state.cli_state.pkg_client, error_handler, NULL);

	if (debug)
		pkgconf_client_set_trace_handler(&state.cli_state.pkg_client, debug_handler, NULL);

	int ret = pkgconf_cli_run(&state.cli_state, test_argc, test_argv, 1);
	pkgconf_argv_free(test_argv);

	if (pkgconf_buffer_len(&out->o_stdout))
		pkgconf_buffer_trim_byte(&out->o_stdout);

	if (pkgconf_buffer_len(&out->o_stderr))
		pkgconf_buffer_trim_byte(&out->o_stderr);

	PKGCONF_FOREACH_LIST_ENTRY(testcase->expected_stdout.head, n)
	{
		pkgconf_test_bufferset_t *set = n->data;

		if (!test_match_buffer(testcase->match_stdout, &set->buffer, &out->o_stdout, "stdout"))
			passed = false;
	}

	PKGCONF_FOREACH_LIST_ENTRY(testcase->expected_stderr.head, n)
	{
		pkgconf_test_bufferset_t *set = n->data;

		if (!test_match_buffer(testcase->match_stderr, &set->buffer, &out->o_stderr, "stderr"))
			passed = false;
	}

	if (ret != testcase->exitcode)
	{
		fprintf(stderr, "exitcode %d does not match expected %d\n", ret, testcase->exitcode);
		passed = false;
	}

	printf("%s: %s\n", passed ? "PASS" : "FAIL", testcase->name);

	if (!passed)
		annotate_result(testcase, ret, out);

	test_output_reset(out);

	return passed;
}

void
free_test_case(pkgconf_test_case_t *testcase)
{
	test_bufferset_free(&testcase->define_variables);
	test_bufferset_free(&testcase->expected_stderr);
	test_bufferset_free(&testcase->expected_stdout);

	test_environment_free(&testcase->env_vars);
	pkgconf_path_free(&testcase->search_path);

	pkgconf_buffer_finalize(&testcase->query);
	pkgconf_buffer_finalize(&testcase->want_env_prefix);
	pkgconf_buffer_finalize(&testcase->want_variable);
	pkgconf_buffer_finalize(&testcase->fragment_filter);
	pkgconf_buffer_finalize(&testcase->skip_platforms);
	pkgconf_buffer_finalize(&testcase->atleast_version);
	pkgconf_buffer_finalize(&testcase->exact_version);
	pkgconf_buffer_finalize(&testcase->max_version);

	free(testcase->name);
	free(testcase);
}

bool
process_test_case(char *testcase_file)
{
	pkgconf_test_case_t *testcase = load_test_case(testcase_file);
	bool ret;

	if (testcase == NULL)
	{
		fprintf(stderr, "test %s failed to load\n", testcase_file);
		return false;
	}

	ret = run_test_case(testcase);
	free_test_case(testcase);

	return ret;
}

static inline bool
str_has_suffix(const char *str, const char *suffix)
{
	size_t str_len = strlen(str);
	size_t suf_len = strlen(suffix);

	if (str_len < suf_len)
		return false;

	return !strncasecmp(str + str_len - suf_len, suffix, suf_len);
}

int
path_sort_cmp(const void *a, const void *b)
{
	return strcmp(*(const char **) a, *(const char **) b);
}

bool
process_test_directory(char *dirpath)
{
	bool ret = true;
	DIR *dir = opendir(dirpath);
	if (dir == NULL)
	{
		fprintf(stderr, "failed to open test directory %s\n", dirpath);
		return false;
	}
	char **paths = NULL;
	size_t numpaths = 0;

	struct dirent *dirent;
	for (dirent = readdir(dir); dirent != NULL; dirent = readdir(dir))
	{
		pkgconf_buffer_t pathbuf = PKGCONF_BUFFER_INITIALIZER;

		pkgconf_buffer_append(&pathbuf, dirpath);
		pkgconf_buffer_append(&pathbuf, "/");
		pkgconf_buffer_append(&pathbuf, dirent->d_name);

		char *pathstr = pkgconf_buffer_freeze(&pathbuf);
		if (pathstr == NULL)
			continue;

		if (!str_has_suffix(pathstr, ".test"))
		{
			free(pathstr);
			continue;
		}

		paths = pkgconf_reallocarray(paths, ++numpaths, sizeof(void *));
		paths[numpaths - 1] = pathstr;
	}

	qsort(paths, numpaths, sizeof(void *), path_sort_cmp);

	for (size_t i = 0; i < numpaths; i++)
	{
		char *pathstr = paths[i];

		ret = process_test_case(pathstr);
		free(pathstr);

		if (!ret)
			break;
	}

	free(paths);
	closedir(dir);
	return ret;
}

void
usage(void)
{
	fprintf(stderr, "usage: test-runner --test-fixtures <path-to-fixtures> <path-to-tests>\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int ret;

	struct pkg_option options[] = {
		{"test-fixtures", required_argument, NULL, 1},
		{"debug", no_argument, NULL, 2},
		{"test-case", required_argument, NULL, 3},
		{NULL, 0, NULL, 0},
	};
	char *testcase = NULL;

	while ((ret = pkg_getopt_long_only(argc, argv, "", options, NULL)) != -1)
	{
		switch (ret)
		{
		case 1:
			test_fixtures_dir = pkg_optarg;
			break;
		case 2:
			debug = true;
			break;
		case 3:
			testcase = pkg_optarg;
			break;
		}
	}

	if (test_fixtures_dir == NULL)
		usage();

	if (testcase != NULL)
		return process_test_case(testcase) ? EXIT_SUCCESS : EXIT_FAILURE;

	if (argv[pkg_optind] == NULL)
		usage();

	return process_test_directory(argv[pkg_optind]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
