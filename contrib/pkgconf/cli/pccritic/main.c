/*
 * pccritic/main.c
 * pc(5) quality scorer
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2026 pkgconf authors (see AUTHORS).
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
#include "critic.h"

#define PKG_VERSION		(((uint64_t) 1) << 1)
#define PKG_ABOUT		(((uint64_t) 1) << 2)
#define PKG_HELP		(((uint64_t) 1) << 3)

#define PKG_QUIET		100
#define PKG_MIN_SCORE		101
#define PKG_DEFINE_VARIABLE	102
#define PKG_COLOR		103

#define ANSI_RESET	"\033[0m"
#define ANSI_DIM	"\033[2m"

typedef enum {
	COLOR_AUTO = 0,
	COLOR_ALWAYS,
	COLOR_NEVER,
} color_mode_t;

static pkgconf_client_t pkg_client;
static uint64_t want_flags;
static bool opt_quiet = false;
static int opt_min_score = -1;
static color_mode_t opt_color = COLOR_AUTO;
static bool color_on = false;
static FILE *out = NULL;
static FILE *error_msgout = NULL;

static bool
parse_min_score(const char *value, int *score)
{
	char *end;
	long parsed;

	errno = 0;
	parsed = strtol(value, &end, 10);

	if (value == end || *end != '\0' || errno == ERANGE || parsed < 0 || parsed > 100)
		return false;

	*score = (int) parsed;
	return true;
}

static void
resolve_color(void)
{
	switch (opt_color)
	{
	case COLOR_ALWAYS:
		color_on = true;
		break;
	case COLOR_NEVER:
		color_on = false;
		break;
	case COLOR_AUTO:
	default:
		color_on = isatty(fileno(out)) && getenv("NO_COLOR") == NULL;
		break;
	}
}

static const char *
severity_color(pccritic_severity_t severity)
{
	if (!color_on)
		return "";

	switch (severity)
	{
	case PCCRITIC_SEV_CRITICAL:	return "\033[1;31m";	/* bold red */
	case PCCRITIC_SEV_MAJOR:	return "\033[31m";	/* red */
	case PCCRITIC_SEV_MINOR:	return "\033[33m";	/* yellow */
	case PCCRITIC_SEV_INFO:		return "\033[36m";	/* cyan */
	default:			return "";
	}
}

static const char *
grade_color(char grade)
{
	if (!color_on)
		return "";

	switch (grade)
	{
	case 'A':
	case 'B':	return "\033[32m";	/* green */
	case 'C':
	case 'D':	return "\033[33m";	/* yellow */
	default:	return "\033[1;31m";	/* bold red */
	}
}

static const char *
dim(void)
{
	return color_on ? ANSI_DIM : "";
}

static const char *
reset(void)
{
	return color_on ? ANSI_RESET : "";
}

static const char *
environ_lookup_handler(const pkgconf_client_t *client, const char *key)
{
	(void) client;
	return getenv(key);
}

static bool
error_handler(const char *msg, const pkgconf_client_t *client, void *data)
{
	(void) client;
	(void) data;
	return pkgconf_output_file_fmt(error_msgout, "%s", msg);
}

/* pccritic emits its own findings; swallow libpkgconf's own warnings so they
 * do not interleave with the report.
 */
static bool
silent_handler(const char *msg, const pkgconf_client_t *client, void *data)
{
	(void) msg;
	(void) client;
	(void) data;
	return true;
}

static int
version(void)
{
	printf("pccritic %s\n", PACKAGE_VERSION);
	return EXIT_SUCCESS;
}

static int
about(void)
{
	printf("pccritic (%s %s)\n", PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright (c) 2011-2026 pkgconf authors (see AUTHORS in documentation directory)\n\n");
	printf("Permission to use, copy, modify, and/or distribute this software for any\n");
	printf("purpose with or without fee is hereby granted, provided that the above\n");
	printf("copyright notice and this permission notice appear in all copies.\n\n");
	printf("This software is provided 'as is' and without any warranty, express or\n");
	printf("implied.  In no event shall the authors be liable for any damages arising\n");
	printf("from the use of this software.\n\n");
	printf("Report bugs at <%s>.\n", PACKAGE_BUGREPORT);
	return EXIT_SUCCESS;
}

static int
usage(void)
{
	printf("usage: pccritic [--flags] [module-or-file ...]\n");

	printf("\npccritic scores the quality of pkg-config (pc(5)) files.  Each argument\n");
	printf("is either a module name resolved from the search path, or a path to a\n");
	printf(".pc file.  Findings are reported and the file is graded from A to F.\n");

	printf("\nbasic options:\n\n");

	printf("  --help                            this message\n");
	printf("  --about                           print pccritic version and license to stdout\n");
	printf("  --version                         print pccritic version to stdout\n");
	printf("  --quiet                           print only the score and grade per target\n");
	printf("  --min-score N                     exit non-zero if any target scores below N\n");
	printf("  --color[=WHEN]                    colorize output; WHEN is 'auto', 'always' or 'never'\n");
	printf("  --define-variable=varname=value   define variable 'varname' as 'value'\n");

	return EXIT_SUCCESS;
}

static bool
ends_with(const char *str, const char *suffix)
{
	size_t slen = strlen(str), xlen = strlen(suffix);

	return slen >= xlen && strcmp(str + slen - xlen, suffix) == 0;
}

static bool
looks_like_path(const char *arg)
{
	return strchr(arg, '/') != NULL || ends_with(arg, ".pc");
}

static bool
file_readable(const char *path)
{
	FILE *f = fopen(path, "rb");

	if (f == NULL)
		return false;

	fclose(f);
	return true;
}

static pkgconf_pkg_t *
resolve_target(pkgconf_client_t *client, const char *arg)
{
	if (looks_like_path(arg))
		return pkgconf_pkg_new_from_path(client, arg, 0);

	return pkgconf_pkg_find(client, arg);
}

static void
render_report(const char *target, pkgconf_pkg_t *pkg, pccritic_report_t *report)
{
	pkgconf_node_t *node;

	if (opt_quiet)
	{
		pkgconf_output_file_fmt(out, "%s: %d/100 %s(%c)%s\n",
			target, report->score, grade_color(report->grade), report->grade, reset());
		return;
	}

	pkgconf_output_file_fmt(out, "%s\n", target);
	if (pkg != NULL && pkg->filename != NULL && strcmp(pkg->filename, target) != 0)
		pkgconf_output_file_fmt(out, "  file: %s\n", pkg->filename);

	pkgconf_output_file_fmt(out, "  score: %d/100  grade: %s%c%s  %s(%u critical, %u major, %u minor, %u info)%s\n",
		report->score, grade_color(report->grade), report->grade, reset(),
		dim(),
		report->counts[PCCRITIC_SEV_CRITICAL], report->counts[PCCRITIC_SEV_MAJOR],
		report->counts[PCCRITIC_SEV_MINOR], report->counts[PCCRITIC_SEV_INFO],
		reset());

	if (report->findings.head == NULL)
	{
		pkgconf_output_file_fmt(out, "  no issues found.\n\n");
		return;
	}

	PKGCONF_FOREACH_LIST_ENTRY(report->findings.head, node)
	{
		pccritic_finding_t *finding = node->data;

		pkgconf_output_file_fmt(out, "  %s[%-8s]%s %s %s(%s/%s)%s\n",
			severity_color(finding->severity), pccritic_severity_name(finding->severity), reset(),
			finding->message,
			dim(), pccritic_category_name(finding->category), finding->code, reset());
	}

	pkgconf_output_file_fmt(out, "\n");
}

int
main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;
	int exit_code = EXIT_SUCCESS;
	unsigned int want_client_flags = PKGCONF_PKG_PKGF_SEARCH_PRIVATE | PKGCONF_PKG_PKGF_SKIP_ERRORS;
	pkgconf_cross_personality_t *personality = pkgconf_cross_personality_default();

	out = stdout;
	error_msgout = stderr;

	struct pkg_option options[] = {
		{ "version", no_argument, &want_flags, PKG_VERSION, },
		{ "about", no_argument, &want_flags, PKG_ABOUT, },
		{ "help", no_argument, &want_flags, PKG_HELP, },
		{ "quiet", no_argument, NULL, PKG_QUIET, },
		{ "min-score", required_argument, NULL, PKG_MIN_SCORE, },
		{ "color", optional_argument, NULL, PKG_COLOR, },
		{ "define-variable", required_argument, NULL, PKG_DEFINE_VARIABLE, },
		{ NULL, 0, NULL, 0 }
	};

	while ((ret = pkg_getopt_long_only(argc, argv, "", options, NULL)) != -1)
	{
		switch (ret)
		{
		case PKG_QUIET:
			opt_quiet = true;
			break;
		case PKG_MIN_SCORE:
			if (!parse_min_score(pkg_optarg, &opt_min_score))
			{
				pkgconf_output_file_fmt(error_msgout,
					"pccritic: invalid --min-score value '%s' (use 0 through 100)\n",
					pkg_optarg);
				return EXIT_FAILURE;
			}
			break;
		case PKG_COLOR:
			if (pkg_optarg == NULL || !strcmp(pkg_optarg, "always"))
				opt_color = COLOR_ALWAYS;
			else if (!strcmp(pkg_optarg, "never"))
				opt_color = COLOR_NEVER;
			else if (!strcmp(pkg_optarg, "auto"))
				opt_color = COLOR_AUTO;
			else
			{
				pkgconf_output_file_fmt(error_msgout, "pccritic: invalid --color value '%s' (use auto, always or never)\n", pkg_optarg);
				return EXIT_FAILURE;
			}
			break;
		case PKG_DEFINE_VARIABLE:
			pkgconf_tuple_define_global(&pkg_client, pkg_optarg);
			break;
		case '?':
		case ':':
			return EXIT_FAILURE;
		default:
			break;
		}
	}

	pkgconf_client_options_t client_options = {
		.error_handler = error_handler,
		.personality = personality,
		.environ_lookup_handler = environ_lookup_handler,
	};
	pkgconf_client_init_with_options(&pkg_client, &client_options);

	pkgconf_client_set_warn_handler(&pkg_client, silent_handler, NULL);
	pkgconf_client_set_flags(&pkg_client, want_client_flags);
	pkgconf_client_dir_list_build(&pkg_client, personality);

	if ((want_flags & PKG_ABOUT) == PKG_ABOUT)
	{
		exit_code = about();
		goto out;
	}

	if ((want_flags & PKG_VERSION) == PKG_VERSION)
	{
		exit_code = version();
		goto out;
	}

	if ((want_flags & PKG_HELP) == PKG_HELP)
	{
		exit_code = usage();
		goto out;
	}

	if (pkg_optind >= argc || argv[pkg_optind] == NULL)
	{
		pkgconf_output_file_fmt(error_msgout, "pccritic: please specify at least one module name or .pc file\n");
		exit_code = EXIT_FAILURE;
		goto out;
	}

	resolve_color();

	for (; pkg_optind < argc && argv[pkg_optind] != NULL; pkg_optind++)
	{
		const char *target = argv[pkg_optind];
		pkgconf_pkg_t *pkg;
		pccritic_report_t *report;

		pkg = resolve_target(&pkg_client, target);

		if (pkg == NULL && !(looks_like_path(target) && file_readable(target)))
		{
			pkgconf_output_file_fmt(error_msgout, "pccritic: could not load '%s'\n", target);
			exit_code = EXIT_FAILURE;
			continue;
		}

		report = pccritic_report_new();
		if (report == NULL)
		{
			if (pkg != NULL)
				pkgconf_pkg_unref(&pkg_client, pkg);
			pkgconf_output_file_fmt(error_msgout, "pccritic: out of memory\n");
			exit_code = EXIT_FAILURE;
			goto out;
		}

		if (pkg != NULL)
			pccritic_analyze(&pkg_client, pkg, report);
		else
			pccritic_analyze_unparsable(report, target);

		render_report(target, pkg, report);

		if (opt_min_score >= 0 && report->score < opt_min_score)
			exit_code = EXIT_FAILURE;

		pccritic_report_free(report);
		if (pkg != NULL)
			pkgconf_pkg_unref(&pkg_client, pkg);
	}

out:
	pkgconf_cross_personality_deinit(personality);
	pkgconf_client_deinit(&pkg_client);

	return exit_code;
}
