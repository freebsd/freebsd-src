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
static size_t maximum_package_count = 0;
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
	fprintf(error_msgout, "%s", msg);
	return true;
}

// NOTE: this function is passed to pkgconf_pkg_traverse
static void
generate_spdx_package(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *ptr)
{
	spdxtool_core_spdx_document_t *document = (spdxtool_core_spdx_document_t *)ptr;
	pkgconf_node_t *node = NULL;
	spdxtool_software_sbom_t *sbom = NULL;
	char *package_spdx = NULL;
	char *spdx_id_string = NULL;
	char sep = spdxtool_util_get_uri_separator(client);

	if (pkg->flags & PKGCONF_PKG_PROPF_VIRTUAL)
		return;

	spdx_id_string = spdxtool_util_get_spdx_id_string(client, "software_Sbom", pkg->id);
	if (!spdx_id_string)
		goto err;

	sbom = spdxtool_software_sbom_new(client, spdx_id_string, document->creation_info, "build");
	free(spdx_id_string);
	spdx_id_string = NULL;
	if (!sbom)
		goto err;

	sbom->spdx_document = document;
	sbom->rootElement = pkg;

	package_spdx = spdxtool_util_get_spdx_id_string(client, "Package", pkg->id);
	if (!package_spdx)
		goto err;

	pkgconf_tuple_add(client, &pkg->vars, "spdxId", package_spdx, false, 0);
	free(package_spdx);
	package_spdx = NULL;

	pkgconf_tuple_add(client, &pkg->vars, "creationInfo", document->creation_info, false, 0);
	pkgconf_tuple_add(client, &pkg->vars, "agent", document->agent, false, 0);

	if (pkg->license.head != NULL)
	{
		pkgconf_buffer_t spdx_id_buf = PKGCONF_BUFFER_INITIALIZER;

		pkgconf_buffer_append_fmt(&spdx_id_buf, "%s%chasDeclaredLicense", pkg->id, sep);
		char *spdx_id_name = pkgconf_buffer_freeze(&spdx_id_buf);
		if (!spdx_id_name)
			goto err;

		package_spdx = spdxtool_util_get_spdx_id_string(client, "Relationship", spdx_id_name);
		free(spdx_id_name);
		if (!package_spdx)
			goto err;

		pkgconf_tuple_add(client, &pkg->vars, "hasDeclaredLicense", package_spdx, false, 0);
		free(package_spdx);
		package_spdx = NULL;

		pkgconf_buffer_t concluded_buf = PKGCONF_BUFFER_INITIALIZER;
		pkgconf_buffer_append_fmt(&concluded_buf, "%s%chasConcludedLicense", pkg->id, sep);
		spdx_id_name = pkgconf_buffer_freeze(&concluded_buf);
		if (!spdx_id_name)
			goto err;

		package_spdx = spdxtool_util_get_spdx_id_string(client, "Relationship", spdx_id_name);
		free(spdx_id_name);
		if (!package_spdx)
			goto err;

		pkgconf_tuple_add(client, &pkg->vars, "hasConcludedLicense", package_spdx, false, 0);
		free(package_spdx);
		package_spdx = NULL;

		PKGCONF_FOREACH_LIST_ENTRY(pkg->license.head, node)
		{
			const pkgconf_license_t *license = node->data;
			if (license->type == PKGCONF_LICENSE_EXPRESSION)
			{
				if (!spdxtool_core_spdx_document_add_license(client, document, license->data))
					goto err;
			}
		}
	}

	node = calloc(1, sizeof(pkgconf_node_t));
	if (!node)
		goto err;

	pkgconf_node_insert_tail(node, sbom, &document->rootElement);
	return;

err:
	pkgconf_error(client, "generate_spdx_package: failed for %s", pkg->id);
	free(package_spdx);
	free(spdx_id_string);
	spdxtool_software_sbom_free(sbom);
}

static bool
generate_spdx(pkgconf_client_t *client, pkgconf_pkg_t *world, const char *creation_time, const char *creation_id, const char *agent_name)
{
	const char *agent_name_string = agent_name ? agent_name : "Default";
	const char *creation_id_string = creation_id ? creation_id : "_:creationinfo_1";

	spdxtool_core_agent_t *agent = spdxtool_core_agent_new(client, creation_id_string, agent_name_string);
	if (!agent)
	{
		pkgconf_error(client, "Could not create agent struct");
		return false;
	}

	spdxtool_core_creation_info_t *creation = spdxtool_core_creation_info_new(client, agent->spdx_id, creation_id_string, creation_time);
	if (!creation)
	{
		pkgconf_error(client, "Could not create creation info struct");
		spdxtool_core_agent_free(agent);
		return false;
	}

	char *spdx_id_int = spdxtool_util_get_spdx_id_int(client, "spdxDocument");
	spdxtool_core_spdx_document_t *document = spdxtool_core_spdx_document_new(client, spdx_id_int, creation_id_string, agent->spdx_id);
	free(spdx_id_int);
	if (!document)
	{
		pkgconf_error(client, "Could not create document");
		spdxtool_core_creation_info_free(creation);
		spdxtool_core_agent_free(agent);
		return false;
	}

	int eflag = pkgconf_pkg_traverse(client, world, generate_spdx_package, document, maximum_traverse_depth, 0);
	if (eflag != PKGCONF_PKG_ERRF_OK)
	{
		spdxtool_core_spdx_document_free(document);
		spdxtool_core_creation_info_free(creation);
		spdxtool_core_agent_free(agent);
		return false;
	}

    spdxtool_serialize_value_t *root = spdxtool_serialize_sbom(client, agent, creation, document);
	if (!root)
	{
		spdxtool_core_spdx_document_free(document);
		spdxtool_core_creation_info_free(creation);
		spdxtool_core_agent_free(agent);
		return false;
	}

    pkgconf_buffer_t buffer = PKGCONF_BUFFER_INITIALIZER;
	spdxtool_serialize_value_to_buf(&buffer, root, 0);
	spdxtool_serialize_value_free(root);

	fprintf(sbom_out, "%s\n", pkgconf_buffer_str(&buffer));
	pkgconf_buffer_finalize(&buffer);

	spdxtool_core_spdx_document_free(document);
	spdxtool_core_creation_info_free(creation);
	spdxtool_core_agent_free(agent);
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
				fprintf(stderr, "unable to open %s: %s\n", pkg_optarg, strerror(errno));
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

	while (1)
	{
		const char *package = argv[pkg_optind];

		if (package == NULL)
			break;

		/* check if there is a limit to the number of packages allowed to be included, if so and we have hit
		 * the limit, stop adding packages to the queue.
		 */
		if (maximum_package_count > 0 && pkgq.length > maximum_package_count)
			break;

		while (isspace((unsigned char)package[0]))
			package++;

		/* skip empty packages */
		if (package[0] == '\0')
		{
			pkg_optind++;
			continue;
		}

		if (argv[pkg_optind + 1] == NULL || !PKGCONF_IS_OPERATOR_CHAR(*(argv[pkg_optind + 1])))
		{
			pkgconf_queue_push(&pkgq, package);
			pkg_optind++;
		}
		else
		{
			char packagebuf[PKGCONF_BUFSIZE];

			snprintf(packagebuf, sizeof packagebuf, "%s %s %s", package, argv[pkg_optind + 1], argv[pkg_optind + 2]);
			pkg_optind += 3;

			pkgconf_queue_push(&pkgq, packagebuf);
		}
	}

	if (!pkgconf_queue_solve(&pkg_client, &pkgq, &world, maximum_traverse_depth))
	{
		ret = EXIT_FAILURE;
		goto out;
	}

	spdxtool_util_set_uri_root(&pkg_client, spdx_id_base);
	spdxtool_util_set_uri_separator_colon(&pkg_client, colon_sep);
	spdxtool_util_set_spdx_license(&pkg_client, bom_license);
	spdxtool_util_set_spdx_version(&pkg_client, spdx_version);

	if (!generate_spdx(&pkg_client, &world, creation_time, creation_id, agent_name))
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
