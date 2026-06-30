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

#include <libpkgconf/config.h>
#include <libpkgconf/libpkgconf.h>
#include <libpkgconf/stdinc.h>
#include <cli/core.h>
#include <cli/getopt_long.h>
#include <limits.h>
#ifdef _WIN32
#	include <direct.h>
#	include <io.h>
#endif // _WIN32_

#ifndef PKGCONF_LITE
#	if !defined(_WIN32) && !defined(__HAIKU__)
#		define PKGCONF_TEST_PLATFORM "unix"
#	elif !defined(_WIN32)
#		define PKGCONF_TEST_PLATFORM "haiku"
#	else
#		define PKGCONF_TEST_PLATFORM "windows"
#	endif
#else // PKGCONF_LITE
#	define PKGCONF_TEST_PLATFORM "lite"
#endif // PKGCONF_LITE

// Shims shared by both MSVC and MSYS2
#ifdef _WIN32
#	define mkdir(p, m) _mkdir(p)
#	define setenv(n, v, o) _putenv_s(n, v)
#endif // _WIN32

// MSVC-specific shims
#ifdef _MSC_VER
#	define getcwd _getcwd
#	define chdir _chdir
#	define rmdir _rmdir
#	define lstat _lstat
#	define unlink _unlink
#	define popen _popen
#	define pclose _pclose

#	ifndef PATH_MAX
#		define PATH_MAX 32767  // Windows max path for long-path support
#	endif

static char *
mkdtemp(char *tmpl)
{
	if (_mktemp_s(tmpl, strlen(tmpl) + 1) != 0)
		return NULL;
	if (_mkdir(tmpl) != 0)
		return NULL;
	return tmpl;
}
#endif // _MSC_VER

static void test_parser_warn(void *p, const char *fmt, ...) PRINTFLIKE(2, 3);
static void handle_substs(pkgconf_buffer_t *dest, const pkgconf_buffer_t *src, const char *pwd);

static pkgconf_buffer_t test_fixtures_dir = PKGCONF_BUFFER_INITIALIZER;
static pkgconf_buffer_t test_tool_dir = PKGCONF_BUFFER_INITIALIZER;
static bool debug = false;

typedef enum test_match_strategy_
{
	MATCH_EXACT = 0,
	MATCH_PARTIAL,
	MATCH_EMPTY,
} pkgconf_test_match_strategy_t;

typedef struct test_case_
{
	char *name;
	char *testfile_dir;

	pkgconf_list_t search_path;
	pkgconf_buffer_t query;

	pkgconf_list_t expected_stdout;
	pkgconf_test_match_strategy_t match_stdout;

	pkgconf_list_t expected_stderr;
	pkgconf_test_match_strategy_t match_stderr;

	pkgconf_buffer_t expected_stdout_file;

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

	pkgconf_buffer_t tool;
	pkgconf_buffer_t tool_args; // TODO: tool-specific flags

	pkgconf_list_t mkdirs;
#ifndef _WIN32
	pkgconf_list_t symlinks;
#endif
	pkgconf_list_t copies;

#ifndef PKGCONF_LITE
	pkgconf_buffer_t want_personality;
#endif
} pkgconf_test_case_t;

typedef struct test_state_
{
	pkgconf_cli_state_t cli_state;
	const pkgconf_test_case_t *testcase;
} pkgconf_test_state_t;

typedef struct test_environ_
{
	pkgconf_node_t node;
	char *key;
	char *value;
} pkgconf_test_environ_t;

typedef struct test_output_
{
	pkgconf_output_t output;

	pkgconf_buffer_t o_stdout;
	pkgconf_buffer_t o_stderr;
} pkgconf_test_output_t;

typedef struct test_flag_pair_
{
	const char *name;
	uint64_t flag;
} pkgconf_test_flag_pair_t;

typedef void (*test_keyword_func_t)(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, const char *value);

typedef struct test_keyword_pair_
{
	const char *keyword;
	const test_keyword_func_t func;
	const ptrdiff_t offset;
} pkgconf_test_keyword_pair_t;

static void
test_environment_push(pkgconf_test_case_t *testcase, const char *key, const char *value)
{
	pkgconf_test_environ_t *env = calloc(1, sizeof(*env));
	if (env == NULL)
		return;

	env->key = strdup(key);
	env->value = strdup(value);
	pkgconf_node_insert_tail(&env->node, env, &testcase->env_vars);
}

static void
test_environment_free(pkgconf_list_t *env_list)
{
	pkgconf_node_t *iter, *iter_next;

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(env_list->head, iter_next, iter)
	{
		pkgconf_test_environ_t *env = iter->data;

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
	pkgconf_node_t *iter;

	PKGCONF_FOREACH_LIST_ENTRY(state->testcase->env_vars.head, iter)
	{
		pkgconf_test_environ_t *env = iter->data;

		if (!strcmp(key, env->key))
		{
			char cwd[PATH_MAX] = {0};
			const char *pwd = getcwd(cwd, sizeof(cwd));

			pkgconf_buffer_t expanded = PKGCONF_BUFFER_INITIALIZER;
			handle_substs(&expanded, PKGCONF_BUFFER_FROM_STR(env->value), pwd);

			free(env->value);
			env->value = strdup(pkgconf_buffer_str_or_empty(&expanded));
			pkgconf_buffer_finalize(&expanded);

			return env->value;
		}
	}

	return NULL;
}

#ifndef PKGCONF_LITE
static bool
debug_handler(const char *msg, const pkgconf_client_t *client, void *data)
{
	(void) client;
	(void) data;
	fprintf(stderr, "%s", msg);
	return true;
}
#endif // PKGCONF_LITE

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
	static pkgconf_test_output_t output =
	{
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

/*
 * handle_substs: expand %TEST_FIXTURES_DIR%, %DIR_SEP%, and %PWD%
 * in src into dest. pwd may be NULL, in which case %PWD% is left as-is
 * (it should only appear in fields that are re-expanded after tmp_dir creation).
 */
static void
handle_substs(pkgconf_buffer_t *dest, const pkgconf_buffer_t *src, const char *pwd)
{
	struct subst_pair
	{
		const char *key;
		const char *value;
	} subst_pairs[] =
	{
		{"%TEST_FIXTURES_DIR%",	pkgconf_buffer_str(&test_fixtures_dir)},
		{"%DIR_SEP%",		PKG_CONFIG_PATH_SEP_S},
		{"%PWD%",		pwd != NULL ? pwd : "%PWD%"},
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

static int
test_keyword_pair_cmp(const void *key, const void *ptr)
{
	const pkgconf_test_keyword_pair_t *pair = ptr;
	return strcasecmp(key, pair->keyword);
}

static void
test_keyword_set_int(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, const char *value)
{
	(void) keyword;
	(void) warnprefix;

	int *dest = (int *)((char *) testcase + offset);
	*dest = atoi(value);
}

static void
test_keyword_set_buffer(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, const char *value)
{
	(void) keyword;
	(void) warnprefix;

	pkgconf_buffer_t *dest = (pkgconf_buffer_t *)((char *) testcase + offset);
	handle_substs(dest, PKGCONF_BUFFER_FROM_STR((char *) value), NULL);
}

static void
test_keyword_extend_bufferset(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, const char *value)
{
	(void) keyword;
	(void) warnprefix;

	pkgconf_list_t *dest = (pkgconf_list_t *)((char *) testcase + offset);
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	handle_substs(&buf, PKGCONF_BUFFER_FROM_STR((char *) value), NULL);
	pkgconf_bufferset_extend(dest, &buf);
	pkgconf_buffer_finalize(&buf);
}

static int
test_flag_pair_cmp(const void *key, const void *ptr)
{
	const pkgconf_test_flag_pair_t *pair = ptr;
	return strcasecmp(key, pair->name);
}

static const pkgconf_test_flag_pair_t test_flag_pairs[] =
{
	{"cflags",			PKG_CFLAGS},
	{"cflags-only-i",		PKG_CFLAGS_ONLY_I},
	{"cflags-only-other",		PKG_CFLAGS_ONLY_OTHER},
	{"debug",			PKG_DEBUG},
	{"define-prefix",		PKG_DEFINE_PREFIX},
	{"digraph",			PKG_DIGRAPH},
	{"dont-define-prefix",		PKG_DONT_DEFINE_PREFIX},
	{"dont-relocate-paths",		PKG_DONT_RELOCATE_PATHS},
	{"dump-license",		PKG_DUMP_LICENSE},
	{"dump-license-file",		PKG_DUMP_LICENSE_FILE},
	{"dump-personality",		PKG_DUMP_PERSONALITY},
	{"dump-source",			PKG_DUMP_SOURCE},
	{"env-only",			PKG_ENV_ONLY},
	{"errors-on-stdout",		PKG_ERRORS_ON_STDOUT},
	{"exists",			PKG_EXISTS},
	{"exists-cflags",		PKG_EXISTS_CFLAGS},
	{"fragment-tree",		PKG_FRAGMENT_TREE},
	{"ignore-conflicts",		PKG_IGNORE_CONFLICTS},
	{"internal-cflags",		PKG_INTERNAL_CFLAGS},
	{"keep-system-cflags",		PKG_KEEP_SYSTEM_CFLAGS},
	{"keep-system-libs",		PKG_KEEP_SYSTEM_LIBS},
	{"libs",			PKG_LIBS},
	{"libs-only-ldpath",		PKG_LIBS_ONLY_LDPATH},
	{"libs-only-libname",		PKG_LIBS_ONLY_LIBNAME},
	{"libs-only-other",		PKG_LIBS_ONLY_OTHER},
	{"list",			PKG_LIST},
	{"list-package-names",		PKG_LIST_PACKAGE_NAMES},
	{"modversion",			PKG_MODVERSION},
	{"msvc-syntax",			PKG_MSVC_SYNTAX},
	{"newlines",			PKG_NEWLINES},
	{"no-cache",			PKG_NO_CACHE},
	{"no-provides",			PKG_NO_PROVIDES},
	{"no-uninstalled",		PKG_NO_UNINSTALLED},
	{"path",			PKG_PATH},
	{"print-digraph-query-nodes",	PKG_PRINT_DIGRAPH_QUERY_NODES},
	{"print-errors",		PKG_PRINT_ERRORS},
	{"print-provides",		PKG_PROVIDES},
	{"print-requires",		PKG_REQUIRES},
	{"print-requires-private",	PKG_REQUIRES_PRIVATE},
	{"print-variables",		PKG_VARIABLES},
	{"pure",			PKG_PURE},
	{"shared",			PKG_SHARED},
	{"short-errors",		PKG_SHORT_ERRORS},
	{"silence-errors",		PKG_SILENCE_ERRORS},
	{"simulate",			PKG_SIMULATE},
	{"solution",			PKG_SOLUTION},
	{"static",			PKG_STATIC},
	{"uninstalled",			PKG_UNINSTALLED},
	{"validate",			PKG_VALIDATE},
};

static void
test_keyword_set_wanted_flags(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, const char *value)
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
		pkgconf_buffer_push_byte(&pathbuf, '/');
		pkgconf_buffer_append(&pathbuf, p);
		pkgconf_path_add(pkgconf_buffer_str(&pathbuf), dirlist, false);
		pkgconf_buffer_finalize(&pathbuf);

		count++, iter = NULL;
	}
	free(workbuf);

	return count;
}

static void
test_keyword_set_path_list(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, const char *value)
{
	(void) keyword;
	(void) warnprefix;

	pkgconf_list_t *dest = (pkgconf_list_t *)((char *) testcase + offset);
	prefixed_path_split(value, dest, pkgconf_buffer_str(&test_fixtures_dir));
}

static void
test_keyword_set_match_strategy(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, const char *value)
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
test_keyword_set_environment(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, const char *value)
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

	// store raw, vars are expanded at run time
	test_environment_push(testcase, value, eq);
}

#ifdef _WIN32
static void
test_keyword_disabled(pkgconf_test_case_t *testcase, const char *keyword, const char *warnprefix, const ptrdiff_t offset, const char *value)
{
	(void) testcase;
	(void) keyword;
	(void) warnprefix;
	(void) offset;
	(void) value;
}
#endif

static const pkgconf_test_keyword_pair_t test_keyword_pairs[] =
{
	{"AtLeastVersion",	test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, atleast_version)},
	{"DefineVariable",	test_keyword_extend_bufferset,		offsetof(pkgconf_test_case_t, define_variables)},
	{"Environment",		test_keyword_set_environment,		offsetof(pkgconf_test_case_t, env_vars)},
	{"ExactVersion",	test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, exact_version)},
	{"ExpectedExitCode",	test_keyword_set_int,			offsetof(pkgconf_test_case_t, exitcode)},
	{"ExpectedStderr",	test_keyword_extend_bufferset,		offsetof(pkgconf_test_case_t, expected_stderr)},
	{"ExpectedStdout",	test_keyword_extend_bufferset,		offsetof(pkgconf_test_case_t, expected_stdout)},
	{"ExpectedStdoutFile",	test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, expected_stdout_file)},
	{"FragmentFilter",	test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, fragment_filter)},
	{"MatchStderr",		test_keyword_set_match_strategy,	offsetof(pkgconf_test_case_t, match_stderr)},
	{"MatchStdout",		test_keyword_set_match_strategy,	offsetof(pkgconf_test_case_t, match_stdout)},
	{"MaxVersion",		test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, max_version)},
	{"PackageSearchPath",	test_keyword_set_path_list,		offsetof(pkgconf_test_case_t, search_path)},
	{"Query",		test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, query)},
	{"SetupCopy",		test_keyword_extend_bufferset,		offsetof(pkgconf_test_case_t, copies)},
	{"SetupMkdir",		test_keyword_extend_bufferset,		offsetof(pkgconf_test_case_t, mkdirs)},
#ifdef _WIN32
	{"SetupSymlink",	test_keyword_disabled,			0},
#else
	{"SetupSymlink",	test_keyword_extend_bufferset,		offsetof(pkgconf_test_case_t, symlinks)},
#endif
	{"SkipPlatforms",	test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, skip_platforms)},
	{"Tool",		test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, tool)},
	{"ToolArgs",		test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, tool_args)},
	{"VerbosityLevel",	test_keyword_set_int,			offsetof(pkgconf_test_case_t, verbosity)},
	{"WantedFlags",		test_keyword_set_wanted_flags,		offsetof(pkgconf_test_case_t, wanted_flags)},
	{"WantEnvPrefix",	test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, want_env_prefix)},
#ifndef PKGCONF_LITE
	{"WantPersonality",	test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, want_personality)},
#endif
	{"WantVariable",	test_keyword_set_buffer,		offsetof(pkgconf_test_case_t, want_variable)},
};

static void
test_keyword_set(void *data, const char *warnprefix, const char *keyword, const char *value)
{
	pkgconf_test_case_t *testcase = data;
	const pkgconf_test_keyword_pair_t *pair = bsearch(keyword,
		test_keyword_pairs, PKGCONF_ARRAY_SIZE(test_keyword_pairs),
		sizeof(*pair), test_keyword_pair_cmp);

	if (pair == NULL || pair->func == NULL)
		return;

	pair->func(testcase, warnprefix, keyword, pair->offset, value);
}

static const pkgconf_parser_operand_func_t test_parser_ops[256] =
{
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

static pkgconf_test_case_t *
load_test_case(char *testfile)
{
	FILE *testf = fopen(testfile, "r");
	if (testf == NULL)
		return NULL;

	pkgconf_test_case_t *out = calloc(1, sizeof(*out));
	if (out == NULL)
		goto cleanup;

	char *nameptr;
	if ((nameptr = strrchr(testfile, '/')) != NULL)
		nameptr++;
	else
		nameptr = testfile;

	out->name = strdup(nameptr);

	// store directory containing the test file for ExpectedStdoutFile resolution
	{
		char *dirend = strrchr(testfile, '/');
		if (dirend != NULL)
		{
			size_t dirlen = (size_t)(dirend - testfile);
			out->testfile_dir = calloc(1, dirlen + 1);
			if (out->testfile_dir != NULL)
				memcpy(out->testfile_dir, testfile, dirlen);
		}
		else
			out->testfile_dir = strdup(".");
	}

	pkgconf_parser_parse(testf, out, test_parser_ops, test_parser_warn, testfile);

cleanup:
	fclose(testf);
	return out;
}

// we use a custom personality to ensure the tests are fully hermetic
static pkgconf_cross_personality_t *
personality_for_test(const pkgconf_test_case_t *testcase)
{
#ifndef PKGCONF_LITE
	if (pkgconf_buffer_len(&testcase->want_personality))
		return pkgconf_cross_personality_find(pkgconf_buffer_str(&testcase->want_personality));
#endif

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

static bool
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

static bool
read_file_into_buffer(FILE *f, pkgconf_buffer_t *buf)
{
	char tmp[4096] = {0};
	size_t n;
	while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0)
		pkgconf_buffer_append_slice(buf, tmp, n);

	if (ferror(f))
		return false;

	return true;
}

static bool
open_file_into_buffer(const char *path, pkgconf_buffer_t *buf)
{
	FILE *f = fopen(path, "r");
	if (f == NULL)
		return false;

	bool ok = read_file_into_buffer(f, buf);
	fclose(f);
	return ok;
}

static bool
copy_file(const char *dst, const char *src)
{
	FILE *fsrc = fopen(src, "rb");
	if (!fsrc)
		return false;

	FILE *fdst = fopen(dst, "wb");
	if (!fdst)
	{
		int errno_save = errno;
		fclose(fsrc);
		errno = errno_save;
		return false;
	}

	bool ok = true;
	char buf[4096] = {0};
	size_t nr;
	while ((nr = fread(buf, 1, sizeof(buf), fsrc)) > 0)
	{
		if (fwrite(buf, 1, nr, fdst) != nr)
		{
			ok = false;
			break;
		}
	}

	if (ferror(fsrc) || ferror(fdst))
		ok = false;

	int errno_save = errno;
	fclose(fsrc);
	fclose(fdst);
	errno = errno_save;

	return ok;
}

/*
 * Recursively remove a directory tree.
 */
static void
rmdir_recursive(const char *path)
{
	DIR *dir = opendir(path);
	if (dir == NULL)
		return;

	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL)
	{
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;

		pkgconf_buffer_t child = PKGCONF_BUFFER_INITIALIZER;
		pkgconf_buffer_append(&child, path);
		pkgconf_buffer_push_byte(&child, '/');
		pkgconf_buffer_append(&child, ent->d_name);

#ifdef _WIN32
		if (_access(pkgconf_buffer_str(&child), 0) == 0)
		{
			DWORD attrs = GetFileAttributesA(pkgconf_buffer_str(&child));
			if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
				rmdir_recursive(pkgconf_buffer_str(&child));
			else
				unlink(pkgconf_buffer_str(&child));
		}
#else
		struct stat st;
		if (lstat(pkgconf_buffer_str(&child), &st) == 0)
		{
			if (S_ISDIR(st.st_mode))
				rmdir_recursive(pkgconf_buffer_str(&child));
			else
				unlink(pkgconf_buffer_str(&child));
		}
#endif

		pkgconf_buffer_finalize(&child);
	}

	closedir(dir);
	rmdir(path);
}

/*
 * Recursively make a directory tree.
 */
static bool
mkdir_recursive(const char *path)
{
	if (!path)
		return false;

	const char *tmpstr = NULL;
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;
	size_t i = 0;

	while (path[i])
	{
		// Append characters up to the next separator
		size_t start = i;
		while (path[i] && path[i] != '/')
			i++;

		pkgconf_buffer_append_slice(&buf, path + start, i - start);

		// If we hit a separator, try to create this component
		if (path[i] == '/')
		{
			pkgconf_buffer_push_byte(&buf, '/');
			tmpstr = pkgconf_buffer_str(&buf);
			if (tmpstr && mkdir(tmpstr, 0755) != 0 && errno != EEXIST)
			{
				pkgconf_buffer_finalize(&buf);
				return false;
			}
			i++; // skip the separator
		}
	}

	// Make the final directory (handles paths without trailing separator)
	tmpstr = pkgconf_buffer_str(&buf);
	bool ok = true;
	if (tmpstr && strlen(tmpstr) > 0)
	{
		if (mkdir(tmpstr, 0755) != 0 && errno != EEXIST)
			ok = false;
	}

	pkgconf_buffer_finalize(&buf);
	return ok;
}

// Returns true if we need tmp_dir
static bool
needs_tmp_dir(const pkgconf_test_case_t *testcase)
{
#ifdef _WIN32
	return testcase->mkdirs.head != NULL || testcase->copies.head != NULL;
#else // _WIN32
	return testcase->mkdirs.head != NULL || testcase->copies.head != NULL || testcase->symlinks.head != NULL;
#endif // _WIN32
}

static int
run_tool(const pkgconf_test_case_t *testcase, pkgconf_buffer_t *o_stdout, pkgconf_buffer_t *o_stderr)
{
	(void) o_stderr; // TODO: external tool stderr goes to real stderr for now

	pkgconf_buffer_t cmdbuf = PKGCONF_BUFFER_INITIALIZER;

	// build: <tool-dir>/<tool> <tool_args>
	if (pkgconf_buffer_len(&test_tool_dir))
	{
		pkgconf_buffer_append(&cmdbuf, pkgconf_buffer_str(&test_tool_dir));
		pkgconf_buffer_push_byte(&cmdbuf, '/');
	}

	pkgconf_buffer_append(&cmdbuf, pkgconf_buffer_str(&testcase->tool));

	if (pkgconf_buffer_len(&testcase->tool_args))
	{
		pkgconf_buffer_append(&cmdbuf, " ");
		pkgconf_buffer_append(&cmdbuf, pkgconf_buffer_str(&testcase->tool_args));
	}

	// Inject Environment vars for the child process
	char tool_cwd[PATH_MAX] = {0};
	const char *pwd = getcwd(tool_cwd, sizeof(tool_cwd));

	pkgconf_node_t *iter;
	PKGCONF_FOREACH_LIST_ENTRY(testcase->env_vars.head, iter)
	{
		pkgconf_test_environ_t *env = iter->data;
		pkgconf_buffer_t expanded = PKGCONF_BUFFER_INITIALIZER;
		handle_substs(&expanded, PKGCONF_BUFFER_FROM_STR(env->value), pwd);
		setenv(env->key, pkgconf_buffer_str_or_empty(&expanded), 1);
		pkgconf_buffer_finalize(&expanded);
	}

	FILE *pipe = popen(pkgconf_buffer_str(&cmdbuf), "r");
	pkgconf_buffer_finalize(&cmdbuf);

	if (pipe == NULL)
	{
		fprintf(stderr, "popen failed for tool '%s': %s\n",
			pkgconf_buffer_str(&testcase->tool), strerror(errno));
		return -1;
	}

	bool ok = read_file_into_buffer(pipe, o_stdout);
	int saved_errno = errno; // pclose() will clobber errno, save it
	int status = pclose(pipe);
	if (!ok)
	{
		fprintf(stderr, "read failed into buffer for command '%s': %s",
			pkgconf_buffer_str(&testcase->tool), strerror(saved_errno));
		return -1;
	}

	if (status == -1)
	{
		fprintf(stderr, "pclose failed for command '%s': %s\n",
			pkgconf_buffer_str(&testcase->tool), strerror(errno));
		return -1;
	}

#if defined(WIFEXITED) && defined(WEXITSTATUS)
	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	fprintf(stderr, "command '%s' did not exit normally\n",
		pkgconf_buffer_str(&testcase->tool));
	return -1;
#else
	return status;
#endif
}

/*
 * Split a bufferset entry on the first space into left and right halves.
 * Caller must free *left_out and *right_out.
 */
static bool
split_pair(const char *entry, char **left_out, char **right_out)
{
	if (entry == NULL)
		return false;

	const char *sp = strchr(entry, ' ');
	if (sp == NULL)
		return false;

	*left_out = pkgconf_strndup(entry, (size_t)(sp - entry));
	*right_out = strdup(sp + 1);
	return true;
}

/*
 * run_setup: execute mkdirs, copies, and symlinks in order.
 * Must be called after chdir() into the tmp_dir.
 */
static bool
run_setup(const pkgconf_test_case_t *testcase, const char *pwd)
{
	pkgconf_node_t *iter;

	// mkdirs: each entry is a single path, relative to tmp_dir
	PKGCONF_FOREACH_LIST_ENTRY(testcase->mkdirs.head, iter)
	{
		pkgconf_bufferset_t *set = iter->data;

		pkgconf_buffer_t path = PKGCONF_BUFFER_INITIALIZER;
		handle_substs(&path, &set->buffer, pwd);

		bool ok = mkdir_recursive(pkgconf_buffer_str(&path));
		pkgconf_buffer_finalize(&path);

		if (!ok && errno != EEXIST)
		{
			fprintf(stderr, "SetupMkdir: mkdir '%s' failed: %s\n",
				pkgconf_buffer_str_or_empty(&set->buffer), strerror(errno));
			return false;
		}
	}

	// copies: "src dst", src relative to TEST_FIXTURES_DIR, dst relative to tmp_dir
	PKGCONF_FOREACH_LIST_ENTRY(testcase->copies.head, iter)
	{
		pkgconf_bufferset_t *set = iter->data;

		pkgconf_buffer_t expanded = PKGCONF_BUFFER_INITIALIZER;
		handle_substs(&expanded, &set->buffer, pwd);

		char *left = NULL, *right = NULL;
		if (!split_pair(pkgconf_buffer_str(&expanded), &left, &right))
		{
			fprintf(stderr, "SetupCopy: malformed entry (expected 'src dst'): %s\n",
				pkgconf_buffer_str_or_empty(&set->buffer));
			pkgconf_buffer_finalize(&expanded);
			return false;
		}
		pkgconf_buffer_finalize(&expanded);

		pkgconf_buffer_t srcpath = PKGCONF_BUFFER_INITIALIZER;
		pkgconf_buffer_append(&srcpath, pkgconf_buffer_str(&test_fixtures_dir));
		pkgconf_buffer_push_byte(&srcpath, '/');
		pkgconf_buffer_append(&srcpath, left);

		bool ok = copy_file(right, pkgconf_buffer_str(&srcpath));
		pkgconf_buffer_finalize(&srcpath);
		if (!ok)
		{
			fprintf(stderr, "SetupCopy: failed to copy file '%s' to '%s': %s\n", left, right, strerror(errno));
			free(left);
			free(right);
			return false;
		}

		free(left);
		free(right);
	}

#ifndef _WIN32
	// symlinks: "target linkpath" — both may be relative to tmp_dir or absolute after %PWD% expansion
	PKGCONF_FOREACH_LIST_ENTRY(testcase->symlinks.head, iter)
	{
		pkgconf_bufferset_t *set = iter->data;

		pkgconf_buffer_t expanded = PKGCONF_BUFFER_INITIALIZER;
		handle_substs(&expanded, &set->buffer, pwd);

		char *target = NULL, *linkpath = NULL;
		if (!split_pair(pkgconf_buffer_str(&expanded), &target, &linkpath))
		{
			fprintf(stderr, "SetupSymlink: malformed entry (expected 'target linkpath'): %s\n",
				pkgconf_buffer_str_or_empty(&set->buffer));
			pkgconf_buffer_finalize(&expanded);
			return false;
		}
		pkgconf_buffer_finalize(&expanded);

		unlink(linkpath);

		if (symlink(target, linkpath) != 0)
		{
			fprintf(stderr, "SetupSymlink: symlink('%s', '%s') failed: %s\n",
				target, linkpath, strerror(errno));
			free(target);
			free(linkpath);
			return false;
		}

		free(target);
		free(linkpath);
	}
#endif // _WIN32

	return true;
}

static void
annotate_result(const pkgconf_test_case_t *testcase, int ret, const pkgconf_test_output_t *out)
{
	pkgconf_buffer_t search_path_buf = PKGCONF_BUFFER_INITIALIZER;
	const pkgconf_node_t *iter;

	PKGCONF_FOREACH_LIST_ENTRY(testcase->search_path.head, iter)
	{
		const pkgconf_path_t *path = iter->data;

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

	PKGCONF_FOREACH_LIST_ENTRY(testcase->env_vars.head, iter)
	{
		const pkgconf_test_environ_t *env = iter->data;

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

	if (pkgconf_buffer_len(&testcase->tool))
		fprintf(stderr, "tool: [%s] tool-args: [%s]\n",
			pkgconf_buffer_str_or_empty(&testcase->tool),
			pkgconf_buffer_str_or_empty(&testcase->tool_args));

	fprintf(stderr, "stdout: [%s]\n",
		pkgconf_buffer_str_or_empty(&out->o_stdout));

	PKGCONF_FOREACH_LIST_ENTRY(testcase->expected_stdout.head, iter)
	{
		pkgconf_bufferset_t *set = iter->data;

		fprintf(stderr,
			"expected-stdout: [%s] (%s)\n",
			pkgconf_buffer_str_or_empty(&set->buffer),
			testcase->match_stdout == MATCH_PARTIAL ? "partial" : "exact");
	}

	if (pkgconf_buffer_len(&testcase->expected_stdout_file))
		fprintf(stderr, "expected-stdout-file: [%s]\n",
			pkgconf_buffer_str_or_empty(&testcase->expected_stdout_file));

	fprintf(stderr, "stderr: [%s]\n",
		pkgconf_buffer_str_or_empty(&out->o_stderr));

	PKGCONF_FOREACH_LIST_ENTRY(testcase->expected_stderr.head, iter)
	{
		pkgconf_bufferset_t *set = iter->data;

		fprintf(stderr,
			"expected-stderr: [%s] (%s)\n",
			pkgconf_buffer_str_or_empty(&set->buffer),
			testcase->match_stderr == MATCH_PARTIAL ? "partial" : "exact");
	}

	PKGCONF_FOREACH_LIST_ENTRY(testcase->define_variables.head, iter)
	{
		pkgconf_bufferset_t *set = iter->data;
		fprintf(stderr, "define-variable: [%s]\n", pkgconf_buffer_str_or_empty(&set->buffer));
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

static bool
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

	// If the test has setup steps, create a new tmp_dir and chdir into it.
	char original_cwd[PATH_MAX] = {0};
	char *tmp_dir = NULL;

	if (getcwd(original_cwd, sizeof(original_cwd)) == NULL)
	{
		fprintf(stderr, "FAIL: getcwd failed: %s\n", strerror(errno));
		return false;
	}

	if (needs_tmp_dir(testcase))
	{
		pkgconf_buffer_t tmp_buf = PKGCONF_BUFFER_INITIALIZER;
		pkgconf_buffer_append_fmt(&tmp_buf, "%s/pkgconf-test-XXXXXX", original_cwd);
		tmp_dir = pkgconf_buffer_freeze(&tmp_buf);

		if (mkdtemp(tmp_dir) == NULL)
		{
			fprintf(stderr, "FAIL: mkdtemp failed: %s\n", strerror(errno));
			free(tmp_dir);
			return false;
		}

		if (chdir(tmp_dir) != 0)
		{
			fprintf(stderr, "FAIL: chdir('%s') failed: %s\n", tmp_dir, strerror(errno));
			rmdir(tmp_dir);
			free(tmp_dir);
			return false;
		}

		if (!run_setup(testcase, tmp_dir))
		{
			fprintf(stderr, "FAIL: %s (setup failed)\n", testcase->name);
			chdir(original_cwd);
			rmdir_recursive(tmp_dir);
			free(tmp_dir);
			return false;
		}
	}

	pkgconf_test_output_t *out = (pkgconf_test_output_t *) test_output();
	int ret;

	if (pkgconf_buffer_len(&testcase->tool))
	{
		ret = run_tool(testcase, &out->o_stdout, &out->o_stderr);
	}
	else
	{
		pkgconf_cross_personality_t *personality = personality_for_test(testcase);
		pkgconf_test_state_t state =
		{
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

		pkgconf_client_init(&state.cli_state.pkg_client, error_handler, NULL, personality, &state, environ_lookup_handler);
		pkgconf_client_set_output(&state.cli_state.pkg_client, &out->output);

		pkgconf_node_t *iter;
		PKGCONF_FOREACH_LIST_ENTRY(testcase->define_variables.head, iter)
		{
			pkgconf_bufferset_t *set = iter->data;
			pkgconf_tuple_define_global(&state.cli_state.pkg_client, pkgconf_buffer_str_or_empty(&set->buffer));
		}

		/*
		 * Re-expand Query now that %PWD% is known (if we have a tmp_dir).
		 * For tests without a tmp_dir this is a no-op since %PWD% won't appear.
		 */
		char query_cwd[PATH_MAX] = {0};
		const char *query_pwd = getcwd(query_cwd, sizeof(query_cwd));

		pkgconf_buffer_t query_expanded = PKGCONF_BUFFER_INITIALIZER;
		handle_substs(&query_expanded, &testcase->query, query_pwd);

		pkgconf_buffer_t arg_buf = PKGCONF_BUFFER_INITIALIZER;
		int test_argc = 0;
		char **test_argv = NULL;

		if (pkgconf_buffer_len(&query_expanded))
			pkgconf_buffer_append_fmt(&arg_buf, "pkgconf %s", pkgconf_buffer_str(&query_expanded));
		else
			pkgconf_buffer_append(&arg_buf, "pkgconf");

		pkgconf_argv_split(pkgconf_buffer_str(&arg_buf), &test_argc, &test_argv);
		pkgconf_buffer_finalize(&arg_buf);
		pkgconf_buffer_finalize(&query_expanded);

		pkgconf_client_set_warn_handler(&state.cli_state.pkg_client, error_handler, NULL);

#ifndef PKGCONF_LITE
		if (debug)
			pkgconf_client_set_trace_handler(&state.cli_state.pkg_client, debug_handler, NULL);
#endif // PKGCONF_LITE

		ret = pkgconf_cli_run(&state.cli_state, test_argc, test_argv, 1);
		pkgconf_argv_free(test_argv);
	}

	if (pkgconf_buffer_len(&out->o_stdout))
		pkgconf_buffer_trim_byte(&out->o_stdout);

	if (pkgconf_buffer_len(&out->o_stderr))
		pkgconf_buffer_trim_byte(&out->o_stderr);

	pkgconf_node_t *iter;

	PKGCONF_FOREACH_LIST_ENTRY(testcase->expected_stdout.head, iter)
	{
		pkgconf_bufferset_t *set = iter->data;

		char expected_cwd[PATH_MAX] = {0};
		const char *expected_pwd = getcwd(expected_cwd, sizeof(expected_cwd));

		pkgconf_buffer_t expected_expanded = PKGCONF_BUFFER_INITIALIZER;
		handle_substs(&expected_expanded, &set->buffer, expected_pwd);

		if (!test_match_buffer(testcase->match_stdout, &expected_expanded, &out->o_stdout, "stdout"))
			passed = false;

		pkgconf_buffer_finalize(&expected_expanded);
	}

	// ExpectedStdoutFile: load file relative to the .test file's directory
	if (pkgconf_buffer_len(&testcase->expected_stdout_file))
	{
		pkgconf_buffer_t filepath = PKGCONF_BUFFER_INITIALIZER;
		pkgconf_buffer_append(&filepath, testcase->testfile_dir);
		pkgconf_buffer_push_byte(&filepath, '/');
		pkgconf_buffer_append(&filepath, pkgconf_buffer_str(&testcase->expected_stdout_file));

		pkgconf_buffer_t file_contents = PKGCONF_BUFFER_INITIALIZER;
		if (!open_file_into_buffer(pkgconf_buffer_str(&filepath), &file_contents))
		{
			fprintf(stderr, "ExpectedStdoutFile: failed to open '%s': %s\n", pkgconf_buffer_str(&filepath), strerror(errno));
			passed = false;
		}
		else
		{
			if (pkgconf_buffer_len(&file_contents))
				pkgconf_buffer_trim_byte(&file_contents);

			if (!test_match_buffer(testcase->match_stdout, &file_contents, &out->o_stdout, "stdout (file)"))
				passed = false;
		}

		pkgconf_buffer_finalize(&file_contents);
		pkgconf_buffer_finalize(&filepath);
	}

	PKGCONF_FOREACH_LIST_ENTRY(testcase->expected_stderr.head, iter)
	{
		pkgconf_bufferset_t *set = iter->data;

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

	// Restore cwd and clean up tmp_dir if we created one
	if (tmp_dir && strcmp(tmp_dir, original_cwd) != 0)
	{
		chdir(original_cwd);
		rmdir_recursive(tmp_dir);
	}

	free(tmp_dir);
	return passed;
}

static void
free_test_case(pkgconf_test_case_t *testcase)
{
	pkgconf_bufferset_free(&testcase->define_variables);
	pkgconf_bufferset_free(&testcase->expected_stderr);
	pkgconf_bufferset_free(&testcase->expected_stdout);
	pkgconf_bufferset_free(&testcase->mkdirs);
#ifndef _WIN32
	pkgconf_bufferset_free(&testcase->symlinks);
#endif // _WIN32
	pkgconf_bufferset_free(&testcase->copies);

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
	pkgconf_buffer_finalize(&testcase->expected_stdout_file);
	pkgconf_buffer_finalize(&testcase->tool);
	pkgconf_buffer_finalize(&testcase->tool_args);

#ifndef PKGCONF_LITE
	pkgconf_buffer_finalize(&testcase->want_personality);
#endif

	free(testcase->name);
	free(testcase->testfile_dir);
	free(testcase);
}

static bool
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

static int
path_sort_cmp(const void *a, const void *b)
{
	return strcmp(*(const char **) a, *(const char **) b);
}

static bool
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
		pkgconf_buffer_push_byte(&pathbuf, '/');
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
		if (!ret)
			break;
	}

	for (size_t i = 0; i < numpaths; i++)
		free(paths[i]);

	free(paths);
	closedir(dir);
	return ret;
}

static void
usage(void)
{
	fprintf(stderr, "usage: test-runner --test-fixtures <path-to-fixtures> [--tool-dir <path>] <path-to-tests>\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int ret;
	char *test_fixtures_dir_arg = NULL;
	char *test_tool_dir_arg = NULL;

	struct pkg_option options[] =
	{
		{"test-fixtures",	required_argument,	NULL,	1},
		{"debug",		no_argument,		NULL,	2},
		{"test-case",		required_argument,	NULL,	3},
		{"tool-dir",		required_argument,	NULL,	4},
		{NULL,			0,			NULL,	0},
	};
	char *testcase = NULL;

	while ((ret = pkg_getopt_long_only(argc, argv, "", options, NULL)) != -1)
	{
		switch (ret)
		{
		case 1:
			test_fixtures_dir_arg = pkg_optarg;
			break;
		case 2:
			debug = true;
			break;
		case 3:
			testcase = pkg_optarg;
			break;
		case 4:
			test_tool_dir_arg = pkg_optarg;
			break;
		}
	}

	if (test_fixtures_dir_arg == NULL)
		usage();

	{
		char test_fixtures_dir_abs[PATH_MAX] = {0};
		if (!realpath(test_fixtures_dir_arg, test_fixtures_dir_abs))
		{
			fprintf(stderr, "realpath failed: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}
		const pkgconf_buffer_t *test_fixtures_dir_arg_buf = PKGCONF_BUFFER_FROM_STR_NONNULL(test_fixtures_dir_abs);
		pkgconf_buffer_subst(&test_fixtures_dir, test_fixtures_dir_arg_buf, "\\", "/");
	}

	if (test_tool_dir_arg != NULL)
	{
		const pkgconf_buffer_t *test_tool_dir_arg_buf = PKGCONF_BUFFER_FROM_STR(test_tool_dir_arg);
		pkgconf_buffer_subst(&test_tool_dir, test_tool_dir_arg_buf, "\\", "/");
	}

	if (testcase != NULL)
		return process_test_case(testcase) ? EXIT_SUCCESS : EXIT_FAILURE;

	if (argv[pkg_optind] == NULL)
		usage();

	return process_test_directory(argv[pkg_optind]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
