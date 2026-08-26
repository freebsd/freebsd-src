/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 */

#include "libpkgconf/config.h"
#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include "getopt_long.h"
#include "util.h"
#include "core.h"
#include "software.h"
#include "serialize.h"
#include "simplelicensing.h"
#include "generate.h"

#define PKG_VERSION			(((uint64_t) 1) << 1)
#define PKG_ABOUT			(((uint64_t) 1) << 2)
#define PKG_HELP			(((uint64_t) 1) << 3)

static const char *spdx_version = "3.0.1";
static const char *bom_license = "CC0-1.0";
static const char *xsd_any_url_default_base = "https://github.com/pkgconf/pkgconf";
static const char *xsd_any_uri_default_base = "github.com:pkgconf:pkgconf";
static int maximum_traverse_depth = 2000;

static pkgconf_client_t pkg_client;
static uint64_t want_flags;
// static int maximum_traverse_depth = 2000;
static FILE *error_msgout = NULL;
static FILE *sbom_out = NULL;

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
	if (!pkgconf_output_file_fmt(error_msgout, "%s", msg))
	{
		pkgconf_error(client, "spdxtool: Could not output error message: %s", strerror(errno));
		return false;
	}
	return true;
}

static int
version(void)
{
	printf("spdxtool %s\n", PACKAGE_VERSION);
	return EXIT_SUCCESS;
}

static int
about(void)
{
	printf("spdxtool (%s %s)\n\n", PACKAGE_NAME, PACKAGE_VERSION);
	printf("SPDX-License-Identifier: BSD-2-Clause\n\n");
	printf("Copyright (c) 2025 The FreeBSD Foundation\n\n");
	printf("Portions of this software were developed by\n");
	printf("Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from\n");
	printf("the FreeBSD Foundation\n\n");
	printf("Report bugs at <%s>.\n", PACKAGE_BUGREPORT);
	return EXIT_SUCCESS;
}

static int
usage(void)
{
	printf("usage: spdxtool [modules]\n");

	printf("\nOptions:\n");

	printf("  --agent-name                      Set agent name [default: 'Default']\n");
	printf("  --creation-time                   Use string as creation time (Should be in ISO8601 format) [default: current time]\n");
	printf("  --creation-id                     Use string as creation id [default: '_:creationinfo_1']\n");
	printf("  --help                            this message\n");
	printf("  --about                           print bomtool version and license to stdout\n");
	printf("  --version                         print bomtool version to stdout\n");
	printf("  --output FILE                     output SBOM data to file\n");
	printf("  --spdx-base-id URL                Uset string as base of SPDX ids [default: %s]\n", xsd_any_uri_default_base);
	printf("  --use-uri                         Use URIs not URLs as SPDX id");
	printf("  --define-variable=varname=value   define variable global 'varname' as 'value'\n");

	return EXIT_SUCCESS;
}

int
main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;
	pkgconf_list_t pkgq = PKGCONF_LIST_INITIALIZER;
	unsigned int want_client_flags = PKGCONF_PKG_PKGF_SEARCH_PRIVATE;
	pkgconf_cross_personality_t *personality = pkgconf_cross_personality_default();
	char *creation_time = NULL;
	char *creation_id = NULL;
	char *agent_name = NULL;
	char world_id[] = "virtual:world";
	char world_realname[] = "virtual world package";
	const char *spdx_id_base = xsd_any_url_default_base;
	bool colon_sep = false;
	pkgconf_pkg_t world =
	{
		.id = world_id,
		.realname = world_realname,
		.flags = PKGCONF_PKG_PROPF_STATIC | PKGCONF_PKG_PROPF_VIRTUAL,
	};

	error_msgout = stderr;
	sbom_out = stdout;

	struct pkg_option options[] =
	{
		{ "agent-name", required_argument, NULL, 100, },
		{ "creation-time", required_argument, NULL, 101, },
		{ "creation-id", required_argument, NULL, 102, },
		{ "version", no_argument, &want_flags, PKG_VERSION, },
		{ "about", no_argument, &want_flags, PKG_ABOUT, },
		{ "help", no_argument, &want_flags, PKG_HELP, },
		{ "output", required_argument, NULL, 103, },
		{ "spdx-base-id", required_argument, NULL, 104, },
		{ "use-uri", no_argument, NULL, 105, },
		{ "define-variable", required_argument, NULL, 106, },
		{ NULL, 0, NULL, 0 }
	};

	while ((ret = pkg_getopt_long_only(argc, argv, "", options, NULL)) != -1)
	{
		switch (ret)
		{
		case 100:
			agent_name = pkg_optarg;
			break;
		case 101:
			creation_time = pkg_optarg;
			break;
		case 102:
			creation_id = pkg_optarg;
			break;
		case 103:
			sbom_out = fopen(pkg_optarg, "w");
			if (sbom_out == NULL)
			{
				pkgconf_output_file_fmt(stderr, "unable to open %s: %s\n", pkg_optarg, strerror(errno));
				return EXIT_FAILURE;
			}
			break;
		case 104:
			spdx_id_base = pkg_optarg;
			break;
		case 105:
			// If SPDX id base have not been altered use default
			if (!strcmp(spdx_id_base, xsd_any_url_default_base))
				spdx_id_base = xsd_any_uri_default_base;
			colon_sep = true;
			break;
		case 106:
			pkgconf_tuple_define_global(&pkg_client, pkg_optarg);
			break;
		case '?':
		case ':':
			return EXIT_FAILURE;
		default:
			break;
		}
	}

	pkgconf_client_init(&pkg_client, error_handler, NULL, personality, NULL, environ_lookup_handler);

	/* we have determined what features we want most likely.  in some cases, we override later. */
	pkgconf_client_set_flags(&pkg_client, want_client_flags);

	/* at this point, want_client_flags should be set, so build the dir list */
	pkgconf_client_dir_list_build(&pkg_client, personality);


	if ((want_flags & PKG_ABOUT) == PKG_ABOUT)
		return about();

	if ((want_flags & PKG_VERSION) == PKG_VERSION)
		return version();

	if ((want_flags & PKG_HELP) == PKG_HELP)
		return usage();

	/* Join the remaining arguments into a single query string, as the main
	 * pkgconf CLI does, and let the dependency parser handle module names,
	 * comparison operators and versions.
	 */
	pkgconf_buffer_t queryparams = PKGCONF_BUFFER_INITIALIZER;

	while (pkg_optind < argc && argv[pkg_optind] != NULL)
	{
		if (pkgconf_buffer_len(&queryparams) > 0)
			pkgconf_buffer_push_byte(&queryparams, ' ');

		pkgconf_buffer_append(&queryparams, argv[pkg_optind]);
		pkg_optind++;
	}

	if (pkgconf_buffer_len(&queryparams) > 0)
		pkgconf_queue_push(&pkgq, pkgconf_buffer_str(&queryparams));

	pkgconf_buffer_finalize(&queryparams);

	if (!pkgconf_queue_solve(&pkg_client, &pkgq, &world, maximum_traverse_depth))
	{
		ret = EXIT_FAILURE;
		goto out;
	}

	spdxtool_util_set_uri_root(&pkg_client, spdx_id_base);
	spdxtool_util_set_uri_separator_colon(&pkg_client, colon_sep);
	spdxtool_util_set_spdx_license(&pkg_client, bom_license);
	spdxtool_util_set_spdx_version(&pkg_client, spdx_version);

	if (!spdxtool_generate(&pkg_client, &world, sbom_out, maximum_traverse_depth, creation_time, creation_id, agent_name))
	{
		ret = EXIT_FAILURE;
		goto out;
	}

	ret = EXIT_SUCCESS;

out:
	pkgconf_solution_free(&pkg_client, &world);
	pkgconf_queue_free(&pkgq);
	pkgconf_cross_personality_deinit(personality);
	pkgconf_client_deinit(&pkg_client);

	return ret;
}
