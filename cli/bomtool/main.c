/*
 * bomtool/main.c
 * main() routine, printer functions
 *
 * Copyright (c) 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
 *     pkgconf authors (see AUTHORS).
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

#define PKG_VERSION			(((uint64_t) 1) << 1)
#define PKG_ABOUT			(((uint64_t) 1) << 2)
#define PKG_HELP			(((uint64_t) 1) << 3)

static const char *spdx_version = "SPDX-2.2";
static const char *bom_license = "CC0-1.0";
static const char *document_ref = "SPDXRef-DOCUMENT";

static pkgconf_client_t pkg_client;
static uint64_t want_flags;
static size_t maximum_package_count = 0;
static int maximum_traverse_depth = 2000;
static FILE *error_msgout = NULL;

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

static const char *
sbom_spdx_identity(pkgconf_pkg_t *pkg)
{
	static char buf[PKGCONF_ITEM_SIZE];

	snprintf(buf, sizeof buf, "%sC64%s", pkg->id, pkg->version);

	return buf;
}

static const char *
sbom_name(pkgconf_pkg_t *world)
{
	static char buf[PKGCONF_BUFSIZE];
	pkgconf_node_t *node;

	pkgconf_strlcpy(buf, "SBOM-SPDX", sizeof buf);

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		pkgconf_pkg_t *match = dep->match;

		if ((dep->flags & PKGCONF_PKG_DEPF_QUERY) != PKGCONF_PKG_DEPF_QUERY)
			continue;

		if (!dep->match)
			continue;

		pkgconf_strlcat(buf, "-", sizeof buf);
		pkgconf_strlcat(buf, sbom_spdx_identity(match), sizeof buf);
	}

	return buf;
}

static void
write_sbom_header(pkgconf_client_t *client, pkgconf_pkg_t *world)
{
	(void) client;
	(void) world;

	printf("SPDXVersion: %s\n", spdx_version);
	printf("DataLicense: %s\n", bom_license);
	printf("SPDXID: %s\n", document_ref);
	printf("DocumentName: %s\n", sbom_name(world));
	printf("DocumentNamespace: https://spdx.org/spdxdocs/bomtool-%s\n", PACKAGE_VERSION);
	printf("Creator: Tool: bomtool %s\n", PACKAGE_VERSION);

	printf("\n\n");
}

static const char *
sbom_identity(pkgconf_pkg_t *pkg)
{
	static char buf[PKGCONF_ITEM_SIZE];

	snprintf(buf, sizeof buf, "%s@%s", pkg->id, pkg->version);

	return buf;
}

static void
write_sbom_package(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *unused)
{
	(void) client;
	(void) unused;

	if (pkg->flags & PKGCONF_PKG_PROPF_VIRTUAL)
		return;

	printf("##### Package: %s\n\n", sbom_identity(pkg));

	printf("PackageName: %s\n", sbom_identity(pkg));
	printf("SPDXID: SPDXRef-Package-%s\n", sbom_spdx_identity(pkg));
	printf("PackageVersion: %s\n", pkg->version);
	printf("PackageDownloadLocation: NOASSERTION\n");
	printf("PackageVerificationCode: NOASSERTION\n");

	/* XXX: What about projects? */
	if (pkg->maintainer != NULL)
		printf("PackageSupplier: Person: %s\n", pkg->maintainer);

	if (pkg->url != NULL)
		printf("PackageHomePage: %s\n", pkg->url);

	printf("PackageLicenseDeclared: %s\n", pkg->license != NULL ? pkg->license : "NOASSERTION");

	if (pkg->copyright != NULL)
		printf("PackageCopyrightText: <text>%s</text>\n", pkg->copyright);

	if (pkg->description != NULL)
		printf("PackageSummary: <text>%s</text>\n", pkg->description);

	if (pkg->source != NULL)
		printf("PackageDownloadLocation: %s\n", pkg->source);

	printf("\n\n");
}

static void
write_sbom_relationships(pkgconf_client_t *client, pkgconf_pkg_t *pkg, void *unused)
{
	(void) client;
	(void) unused;

	char baseref[PKGCONF_ITEM_SIZE];
	pkgconf_node_t *node;

	if (pkg->flags & PKGCONF_PKG_PROPF_VIRTUAL)
		return;

	snprintf(baseref, sizeof baseref, "SPDXRef-Package-%sC64%s", pkg->id, pkg->version);

	PKGCONF_FOREACH_LIST_ENTRY(pkg->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		pkgconf_pkg_t *match = dep->match;

		if (!dep->match)
			continue;

		printf("Relationship: %s DEPENDS_ON SPDXRef-Package-%s\n", baseref, sbom_spdx_identity(match));
		printf("Relationship: SPDXRef-Package-%s DEPENDENCY_OF %s\n", sbom_spdx_identity(match), baseref);
	}

	PKGCONF_FOREACH_LIST_ENTRY(pkg->requires_private.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		pkgconf_pkg_t *match = dep->match;

		if (!dep->match)
			continue;

		printf("Relationship: %s DEPENDS_ON SPDXRef-Package-%s\n", baseref, sbom_spdx_identity(match));
		printf("Relationship: SPDXRef-Package-%s DEV_DEPENDENCY_OF %s\n", sbom_spdx_identity(match), baseref);
	}

	if (pkg->required.head != NULL || pkg->requires_private.head != NULL)
		printf("\n\n");
}

static bool
generate_sbom_from_world(pkgconf_client_t *client, pkgconf_pkg_t *world)
{
	int eflag;
	pkgconf_node_t *node;

	write_sbom_header(client, world);

	eflag = pkgconf_pkg_traverse(client, world, write_sbom_package, NULL, maximum_traverse_depth, 0);
	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	eflag = pkgconf_pkg_traverse(client, world, write_sbom_relationships, NULL, maximum_traverse_depth, 0);
	if (eflag != PKGCONF_PKG_ERRF_OK)
		return false;

	PKGCONF_FOREACH_LIST_ENTRY(world->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		pkgconf_pkg_t *match = dep->match;

		if (!dep->match)
			continue;

		printf("Relationship: %s DESCRIBES SPDXRef-Package-%s\n", document_ref, sbom_spdx_identity(match));
	}

	return true;
}

static int
version(void)
{
	printf("bomtool %s\n", PACKAGE_VERSION);
	return EXIT_SUCCESS;
}

static int
about(void)
{
	printf("bomtool (%s %s)\n", PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright (c) 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021\n");
	printf("    pkgconf authors (see AUTHORS in documentation directory).\n\n");
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
	printf("usage: bomtool [--flags] [modules]\n");

	printf("\nbasic options:\n\n");

	printf("  --help                            this message\n");
	printf("  --about                           print bomtool version and license to stdout\n");
	printf("  --version                         print bomtool version to stdout\n");

	return EXIT_SUCCESS;
}

int
main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;
	pkgconf_list_t pkgq = PKGCONF_LIST_INITIALIZER;
	unsigned int want_client_flags = PKGCONF_PKG_PKGF_SEARCH_PRIVATE;
	pkgconf_cross_personality_t *personality = pkgconf_cross_personality_default();
	pkgconf_pkg_t world = {
		.id = "virtual:world",
		.realname = "virtual world package",
		.flags = PKGCONF_PKG_PROPF_STATIC | PKGCONF_PKG_PROPF_VIRTUAL,
	};

	error_msgout = stderr;

	struct pkg_option options[] = {
		{ "version", no_argument, &want_flags, PKG_VERSION, },
		{ "about", no_argument, &want_flags, PKG_ABOUT, },
		{ "help", no_argument, &want_flags, PKG_HELP, },
		{ NULL, 0, NULL, 0 }
	};

	while ((ret = pkg_getopt_long_only(argc, argv, "", options, NULL)) != -1)
	{
		switch (ret)
		{
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
		if (package[0] == '\0') {
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

	if (pkgq.head == NULL)
	{
		fprintf(stderr, "Please specify at least one package name on the command line.\n");
		ret = EXIT_FAILURE;
		goto out;
	}

	ret = EXIT_SUCCESS;

	if (!pkgconf_queue_solve(&pkg_client, &pkgq, &world, maximum_traverse_depth))
	{
		ret = EXIT_FAILURE;
		goto out;
	}

	if (!generate_sbom_from_world(&pkg_client, &world))
	{
		ret = EXIT_FAILURE;
		goto out;
	}

out:
	pkgconf_solution_free(&pkg_client, &world);
	pkgconf_queue_free(&pkgq);
	pkgconf_cross_personality_deinit(personality);
	pkgconf_client_deinit(&pkg_client);

	return ret;
}
