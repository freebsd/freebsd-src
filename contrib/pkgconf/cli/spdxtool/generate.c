/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 */

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include "util.h"
#include "core.h"
#include "software.h"
#include "serialize.h"
#include "simplelicensing.h"
#include "generate.h"

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

	if (pkg->maintainer != NULL)
	{
		const char *supplier = spdxtool_core_spdx_document_add_maintainer(client, document, pkg->maintainer);
		if (!supplier)
			goto err;

		pkgconf_tuple_add(client, &pkg->vars, "suppliedBy", supplier, false, 0);
	}

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

bool
spdxtool_generate(pkgconf_client_t *client, pkgconf_pkg_t *world, FILE *out, int maxdepth,
	const char *creation_time, const char *creation_id, const char *agent_name)
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

	int eflag = pkgconf_pkg_traverse(client, world, generate_spdx_package, document, maxdepth, 0);
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

	bool ret = pkgconf_output_file_fmt(out, "%s\n", pkgconf_buffer_str(&buffer));
	pkgconf_buffer_finalize(&buffer);

	spdxtool_core_spdx_document_free(document);
	spdxtool_core_creation_info_free(creation);
	spdxtool_core_agent_free(agent);

	if (!ret)
		pkgconf_error(client, "spdxtool: Could not output to file: %s", strerror(errno));
	return ret;
}
