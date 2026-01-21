/*
 * main.c
 * main() routine
 *
 * Copyright (c) 2011-2025 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "libpkgconf/config.h"
#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include "getopt_long.h"
#include "core.h"

static const char *
environ_lookup_handler(const pkgconf_client_t *client, const char *key)
{
	(void) client;

	return getenv(key);
}

static bool
error_handler(const char *msg, const pkgconf_client_t *client, void *data)
{
	(void) data;
	pkgconf_cli_state_t *state = client->client_data;

	fprintf(state->error_msgout, "%s", msg);
	return true;
}

static void
relocate_path(const char *path)
{
	char buf[PKGCONF_BUFSIZE];

	pkgconf_strlcpy(buf, path, sizeof buf);
	pkgconf_path_relocate(buf, sizeof buf);

	printf("%s\n", buf);
}

#ifndef PKGCONF_LITE
static pkgconf_cross_personality_t *
deduce_personality(char *argv[])
{
	const char *argv0 = argv[0];
	char *i, *prefix;
	pkgconf_cross_personality_t *out;

	i = strrchr(argv0, '/');
	if (i != NULL)
		argv0 = i + 1;

#if defined(_WIN32) || defined(_WIN64)
	i = strrchr(argv0, '\\');
	if (i != NULL)
		argv0 = i + 1;
#endif

	i = strstr(argv0, "-pkg");
	if (i == NULL)
		return pkgconf_cross_personality_default();

	prefix = pkgconf_strndup(argv0, i - argv0);
	out = pkgconf_cross_personality_find(prefix);
	free(prefix);
	if (out == NULL)
		return pkgconf_cross_personality_default();

	return out;
}
#endif

static void
version(void)
{
	printf("%s\n", PACKAGE_VERSION);
}

static void
about(void)
{
	printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright (c) 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021\n");
	printf("    pkgconf authors (see AUTHORS in documentation directory).\n\n");
	printf("Permission to use, copy, modify, and/or distribute this software for any\n");
	printf("purpose with or without fee is hereby granted, provided that the above\n");
	printf("copyright notice and this permission notice appear in all copies.\n\n");
	printf("This software is provided 'as is' and without any warranty, express or\n");
	printf("implied.  In no event shall the authors be liable for any damages arising\n");
	printf("from the use of this software.\n\n");
	printf("Report bugs at <%s>.\n", PACKAGE_BUGREPORT);
}

static void
usage(void)
{
	printf("usage: %s [OPTIONS] [LIBRARIES]\n", PACKAGE_NAME);

	printf("\nbasic options:\n\n");

	printf("  --help                            this message\n");
	printf("  --about                           print pkgconf version and license to stdout\n");
	printf("  --version                         print supported pkg-config version to stdout\n");
	printf("  --verbose                         print additional information\n");
	printf("  --atleast-pkgconfig-version       check whether or not pkgconf is compatible\n");
	printf("                                    with a specified pkg-config version\n");
	printf("  --errors-to-stdout                print all errors on stdout instead of stderr\n");
	printf("  --print-errors                    ensure all errors are printed\n");
	printf("  --short-errors                    be less verbose about some errors\n");
	printf("  --silence-errors                  explicitly be silent about errors\n");
	printf("  --list-all                        list all known packages\n");
	printf("  --list-package-names              list all known package names\n");
#ifndef PKGCONF_LITE
	printf("  --simulate                        simulate walking the calculated dependency graph\n");
#endif
	printf("  --no-cache                        do not cache already seen packages when\n");
	printf("                                    walking the dependency graph\n");
	printf("  --log-file=filename               write an audit log to a specified file\n");
	printf("  --with-path=path                  adds a directory to the search path\n");
	printf("  --define-prefix                   override the prefix variable with one that is guessed based on\n");
	printf("                                    the location of the .pc file\n");
	printf("  --dont-define-prefix              do not override the prefix variable under any circumstances\n");
	printf("  --prefix-variable=varname         sets the name of the variable that pkgconf considers\n");
	printf("                                    to be the package prefix\n");
	printf("  --relocate=path                   relocates a path and exits (mostly for testsuite)\n");
	printf("  --dont-relocate-paths             disables path relocation support\n");

#ifndef PKGCONF_LITE
	printf("\ncross-compilation personality support:\n\n");
	printf("  --personality=triplet|filename    sets the personality to 'triplet' or a file named 'filename'\n");
	printf("  --dump-personality                dumps details concerning selected personality\n");
#endif

	printf("\nchecking specific pkg-config database entries:\n\n");

	printf("  --atleast-version                 require a specific version of a module\n");
	printf("  --exact-version                   require an exact version of a module\n");
	printf("  --max-version                     require a maximum version of a module\n");
	printf("  --exists                          check whether or not a module exists\n");
	printf("  --uninstalled                     check whether or not an uninstalled module will be used\n");
	printf("  --no-uninstalled                  never use uninstalled modules when satisfying dependencies\n");
	printf("  --no-provides                     do not use 'provides' rules to resolve dependencies\n");
	printf("  --maximum-traverse-depth          maximum allowed depth for dependency graph\n");
	printf("  --static                          be more aggressive when computing dependency graph\n");
	printf("                                    (for static linking)\n");
	printf("  --shared                          use a simplified dependency graph (usually default)\n");
	printf("  --pure                            optimize a static dependency graph as if it were a normal\n");
	printf("                                    dependency graph\n");
	printf("  --env-only                        look only for package entries in PKG_CONFIG_PATH\n");
	printf("  --ignore-conflicts                ignore 'conflicts' rules in modules\n");
	printf("  --validate                        validate specific .pc files for correctness\n");

	printf("\nquerying specific pkg-config database fields:\n\n");

	printf("  --define-variable=varname=value   define variable 'varname' as 'value'\n");
	printf("  --variable=varname                print specified variable entry to stdout\n");
	printf("  --cflags                          print required CFLAGS to stdout\n");
	printf("  --cflags-only-I                   print required include-dir CFLAGS to stdout\n");
	printf("  --cflags-only-other               print required non-include-dir CFLAGS to stdout\n");
	printf("  --libs                            print required linker flags to stdout\n");
	printf("  --libs-only-L                     print required LDPATH linker flags to stdout\n");
	printf("  --libs-only-l                     print required LIBNAME linker flags to stdout\n");
	printf("  --libs-only-other                 print required other linker flags to stdout\n");
	printf("  --print-requires                  print required dependency frameworks to stdout\n");
	printf("  --print-requires-private          print required dependency frameworks for static\n");
	printf("                                    linking to stdout\n");
	printf("  --print-provides                  print provided dependencies to stdout\n");
	printf("  --print-variables                 print all known variables in module to stdout\n");
#ifndef PKGCONF_LITE
	printf("  --digraph                         print entire dependency graph in graphviz 'dot' format\n");
	printf("  --solution                        print dependency graph solution in a simple format\n");
#endif
	printf("  --keep-system-cflags              keep -I%s entries in cflags output\n", SYSTEM_INCLUDEDIR);
	printf("  --keep-system-libs                keep -L%s entries in libs output\n", SYSTEM_LIBDIR);
	printf("  --path                            show the exact filenames for any matching .pc files\n");
	printf("  --modversion                      print the specified module's version to stdout\n");
	printf("  --internal-cflags                 do not filter 'internal' cflags from output\n");
	printf("  --license                         print the specified module's license to stdout if known\n");
	printf("  --source                          print the specified module's source code location to stdout if known\n");
	printf("  --exists-cflags                   add -DHAVE_FOO fragments to cflags for each found module\n");

	printf("\nfiltering output:\n\n");
#ifndef PKGCONF_LITE
	printf("  --msvc-syntax                     print translatable fragments in MSVC syntax\n");
#endif
	printf("  --fragment-filter=types           filter output fragments to the specified types\n");
	printf("  --env=prefix                      print output as shell-compatible environmental variables\n");
	printf("  --fragment-tree                   visualize printed CFLAGS/LIBS fragments as a tree\n");
	printf("  --newlines                        use newlines for whitespace between fragments\n");

	printf("\nreport bugs to <%s>.\n", PACKAGE_BUGREPORT);
}

int
main(int argc, char *argv[])
{
	int ret;
	pkgconf_cli_state_t state = {
		.want_flags = 0,
	};
	pkgconf_list_t dir_list = PKGCONF_LIST_INITIALIZER;
	char *env_traverse_depth;
	char *logfile_arg = NULL;
	pkgconf_cross_personality_t *personality = NULL;

	if (pkgconf_pledge("stdio rpath wpath cpath unveil", NULL) == -1)
	{
		fprintf(stderr, "pkgconf: pledge failed: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

#ifdef _WIN32
	/* When running regression tests in cygwin, and building native
	 * executable, tests fail unless native executable outputs unix
	 * line endings.  Come to think of it, this will probably help
	 * real people who use cygwin build environments but native pkgconf, too.
	 */
	_setmode(fileno(stdout), O_BINARY);
	_setmode(fileno(stderr), O_BINARY);
#endif

	struct pkg_option options[] = {
		{ "version", no_argument, &state.want_flags, PKG_VERSION|PKG_PRINT_ERRORS, },
		{ "about", no_argument, &state.want_flags, PKG_ABOUT|PKG_PRINT_ERRORS, },
		{ "atleast-version", required_argument, NULL, 2, },
		{ "atleast-pkgconfig-version", required_argument, NULL, 3, },
		{ "libs", no_argument, &state.want_flags, PKG_LIBS|PKG_PRINT_ERRORS, },
		{ "cflags", no_argument, &state.want_flags, PKG_CFLAGS|PKG_PRINT_ERRORS, },
		{ "modversion", no_argument, &state.want_flags, PKG_MODVERSION|PKG_PRINT_ERRORS, },
		{ "variable", required_argument, NULL, 7, },
		{ "exists", no_argument, &state.want_flags, PKG_EXISTS, },
		{ "print-errors", no_argument, &state.want_flags, PKG_PRINT_ERRORS, },
		{ "short-errors", no_argument, &state.want_flags, PKG_SHORT_ERRORS, },
		{ "maximum-traverse-depth", required_argument, NULL, 11, },
		{ "static", no_argument, &state.want_flags, PKG_STATIC, },
		{ "shared", no_argument, &state.want_flags, PKG_SHARED, },
		{ "pure", no_argument, &state.want_flags, PKG_PURE, },
		{ "print-requires", no_argument, &state.want_flags, PKG_REQUIRES, },
		{ "print-variables", no_argument, &state.want_flags, PKG_VARIABLES|PKG_PRINT_ERRORS, },
#ifndef PKGCONF_LITE
		{ "digraph", no_argument, &state.want_flags, PKG_DIGRAPH, },
		{ "solution", no_argument, &state.want_flags, PKG_SOLUTION, },
#endif
		{ "help", no_argument, &state.want_flags, PKG_HELP, },
		{ "env-only", no_argument, &state.want_flags, PKG_ENV_ONLY, },
		{ "print-requires-private", no_argument, &state.want_flags, PKG_REQUIRES_PRIVATE, },
		{ "cflags-only-I", no_argument, &state.want_flags, PKG_CFLAGS_ONLY_I|PKG_PRINT_ERRORS, },
		{ "cflags-only-other", no_argument, &state.want_flags, PKG_CFLAGS_ONLY_OTHER|PKG_PRINT_ERRORS, },
		{ "libs-only-L", no_argument, &state.want_flags, PKG_LIBS_ONLY_LDPATH|PKG_PRINT_ERRORS, },
		{ "libs-only-l", no_argument, &state.want_flags, PKG_LIBS_ONLY_LIBNAME|PKG_PRINT_ERRORS, },
		{ "libs-only-other", no_argument, &state.want_flags, PKG_LIBS_ONLY_OTHER|PKG_PRINT_ERRORS, },
		{ "uninstalled", no_argument, &state.want_flags, PKG_UNINSTALLED, },
		{ "no-uninstalled", no_argument, &state.want_flags, PKG_NO_UNINSTALLED, },
		{ "keep-system-cflags", no_argument, &state.want_flags, PKG_KEEP_SYSTEM_CFLAGS, },
		{ "keep-system-libs", no_argument, &state.want_flags, PKG_KEEP_SYSTEM_LIBS, },
		{ "define-variable", required_argument, NULL, 27, },
		{ "exact-version", required_argument, NULL, 28, },
		{ "max-version", required_argument, NULL, 29, },
		{ "ignore-conflicts", no_argument, &state.want_flags, PKG_IGNORE_CONFLICTS, },
		{ "errors-to-stdout", no_argument, &state.want_flags, PKG_ERRORS_ON_STDOUT, },
		{ "silence-errors", no_argument, &state.want_flags, PKG_SILENCE_ERRORS, },
		{ "list-all", no_argument, &state.want_flags, PKG_LIST|PKG_PRINT_ERRORS, },
		{ "list-package-names", no_argument, &state.want_flags, PKG_LIST_PACKAGE_NAMES|PKG_PRINT_ERRORS, },
#ifndef PKGCONF_LITE
		{ "simulate", no_argument, &state.want_flags, PKG_SIMULATE, },
#endif
		{ "no-cache", no_argument, &state.want_flags, PKG_NO_CACHE, },
		{ "print-provides", no_argument, &state.want_flags, PKG_PROVIDES, },
		{ "no-provides", no_argument, &state.want_flags, PKG_NO_PROVIDES, },
		{ "debug", no_argument, &state.want_flags, PKG_DEBUG|PKG_PRINT_ERRORS, },
		{ "validate", no_argument, &state.want_flags, PKG_VALIDATE|PKG_PRINT_ERRORS|PKG_ERRORS_ON_STDOUT },
		{ "log-file", required_argument, NULL, 40 },
		{ "path", no_argument, &state.want_flags, PKG_PATH },
		{ "with-path", required_argument, NULL, 42 },
		{ "prefix-variable", required_argument, NULL, 43 },
		{ "define-prefix", no_argument, &state.want_flags, PKG_DEFINE_PREFIX },
		{ "relocate", required_argument, NULL, 45 },
		{ "dont-define-prefix", no_argument, &state.want_flags, PKG_DONT_DEFINE_PREFIX },
		{ "dont-relocate-paths", no_argument, &state.want_flags, PKG_DONT_RELOCATE_PATHS },
		{ "env", required_argument, NULL, 48 },
#ifndef PKGCONF_LITE
		{ "msvc-syntax", no_argument, &state.want_flags, PKG_MSVC_SYNTAX },
#endif
		{ "fragment-filter", required_argument, NULL, 50 },
		{ "internal-cflags", no_argument, &state.want_flags, PKG_INTERNAL_CFLAGS },
#ifndef PKGCONF_LITE
		{ "dump-personality", no_argument, &state.want_flags, PKG_DUMP_PERSONALITY },
		{ "personality", required_argument, NULL, 53 },
#endif
		{ "license", no_argument, &state.want_flags, PKG_DUMP_LICENSE },
		{ "license-file", no_argument, &state.want_flags, PKG_DUMP_LICENSE_FILE },
		{ "verbose", no_argument, NULL, 55 },
		{ "exists-cflags", no_argument, &state.want_flags, PKG_EXISTS_CFLAGS },
		{ "fragment-tree", no_argument, &state.want_flags, PKG_FRAGMENT_TREE },
		{ "source", no_argument, &state.want_flags, PKG_DUMP_SOURCE },
		{ "newlines", no_argument, &state.want_flags, PKG_NEWLINES },
		{ NULL, 0, NULL, 0 }
	};

#ifndef PKGCONF_LITE
	if (getenv("PKG_CONFIG_EARLY_TRACE"))
	{
		state.error_msgout = stderr;
		pkgconf_client_set_trace_handler(&state.pkg_client, error_handler, NULL);
	}
#endif

	while ((ret = pkg_getopt_long_only(argc, argv, "", options, NULL)) != -1)
	{
		switch (ret)
		{
		case 2:
			state.required_module_version = pkg_optarg;
			break;
		case 3:
			state.required_pkgconfig_version = pkg_optarg;
			break;
		case 7:
			state.want_variable = pkg_optarg;
			break;
		case 11:
			state.maximum_traverse_depth = atoi(pkg_optarg);
			break;
		case 27:
			pkgconf_tuple_define_global(&state.pkg_client, pkg_optarg);
			break;
		case 28:
			state.required_exact_module_version = pkg_optarg;
			break;
		case 29:
			state.required_max_module_version = pkg_optarg;
			break;
		case 40:
			logfile_arg = pkg_optarg;
			break;
		case 42:
			pkgconf_path_prepend(pkg_optarg, &dir_list, true);
			break;
		case 43:
			pkgconf_client_set_prefix_varname(&state.pkg_client, pkg_optarg);
			break;
		case 45:
			relocate_path(pkg_optarg);
			return EXIT_SUCCESS;
		case 48:
			state.want_env_prefix = pkg_optarg;
			break;
		case 50:
			state.want_fragment_filter = pkg_optarg;
			break;
#ifndef PKGCONF_LITE
		case 53:
			personality = pkgconf_cross_personality_find(pkg_optarg);
			break;
#endif
		case 55:
			state.verbosity++;
			break;
		case '?':
		case ':':
			ret = EXIT_FAILURE;
			goto out;
		default:
			break;
		}
	}

	if (personality == NULL) {
#ifndef PKGCONF_LITE
		personality = deduce_personality(argv);
#else
		personality = pkgconf_cross_personality_default();
#endif
	}

	/* now, bring up the client.  settings are preserved since the client is prealloced */
	pkgconf_client_init(&state.pkg_client, error_handler, &state, personality, &state, environ_lookup_handler);

#ifndef PKGCONF_LITE
	if (getenv("PKG_CONFIG_MSVC_SYNTAX") != NULL)
		state.want_flags |= PKG_MSVC_SYNTAX;
#endif

	if ((env_traverse_depth = getenv("PKG_CONFIG_MAXIMUM_TRAVERSE_DEPTH")) != NULL)
		state.maximum_traverse_depth = atoi(env_traverse_depth);

	if ((state.want_flags & PKG_PRINT_ERRORS) != PKG_PRINT_ERRORS)
		state.want_flags |= (PKG_SILENCE_ERRORS);

	if ((state.want_flags & PKG_SILENCE_ERRORS) == PKG_SILENCE_ERRORS && !getenv("PKG_CONFIG_DEBUG_SPEW"))
		state.want_flags |= (PKG_SILENCE_ERRORS);
	else
		state.want_flags &= ~(PKG_SILENCE_ERRORS);

	if (getenv("PKG_CONFIG_DONT_RELOCATE_PATHS"))
		state.want_flags |= (PKG_DONT_RELOCATE_PATHS);

	if ((state.want_flags & PKG_VALIDATE) == PKG_VALIDATE || (state.want_flags & PKG_DEBUG) == PKG_DEBUG)
		pkgconf_client_set_warn_handler(&state.pkg_client, error_handler, NULL);

#ifndef PKGCONF_LITE
	if ((state.want_flags & PKG_DEBUG) == PKG_DEBUG)
		pkgconf_client_set_trace_handler(&state.pkg_client, error_handler, NULL);
#endif

	pkgconf_path_prepend_list(&state.pkg_client.dir_list, &dir_list);
	pkgconf_path_free(&dir_list);

	if ((state.want_flags & PKG_ABOUT) == PKG_ABOUT)
	{
		about();

		ret = EXIT_SUCCESS;
		goto out;
	}

	if ((state.want_flags & PKG_VERSION) == PKG_VERSION)
	{
		version();

		ret = EXIT_SUCCESS;
		goto out;
	}

	if ((state.want_flags & PKG_HELP) == PKG_HELP)
	{
		usage();

		ret = EXIT_SUCCESS;
		goto out;
	}

	if (logfile_arg == NULL)
		logfile_arg = getenv("PKG_CONFIG_LOG");

	if (logfile_arg != NULL)
	{
		if (pkgconf_unveil(logfile_arg, "rwc") == -1)
		{
			fprintf(stderr, "pkgconf: unveil failed: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}

		state.logfile_out = fopen(logfile_arg, "a");
		pkgconf_audit_set_log(&state.pkg_client, state.logfile_out);
	}

	if (getenv("PKG_CONFIG_ALLOW_SYSTEM_CFLAGS") != NULL)
		state.want_flags |= PKG_KEEP_SYSTEM_CFLAGS;

	if (getenv("PKG_CONFIG_ALLOW_SYSTEM_LIBS") != NULL)
		state.want_flags |= PKG_KEEP_SYSTEM_LIBS;

	return pkgconf_cli_run(&state, argc, argv, pkg_optind);

out:
	pkgconf_cli_state_reset(&state);
	return ret;
}
