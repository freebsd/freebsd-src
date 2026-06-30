/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 */

#include <stdlib.h>
#include <string.h>
#include <libpkgconf/libpkgconf.h>
#include "util.h"
#include "serialize.h"
#include "software.h"
#include "core.h"

/*
 * !doc
 *
 * .. c:function:: spdxtool_software_sbom_t *spdxtool_software_sbom_new(pkgconf_client_t *client, const char *spdx_id, const char *creation_id, const char *sbom_type)
 *
 *    Create new /Software/Sbom struct
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param const char *spdx_id: spdxId for this SBOM element
 *    :param const char *creation_id: id for CreationInfo
 *    :param const char *sbom_type: Sbom types can be found SPDX documention
 *    :return: NULL if some problem occurs and Sbom struct if not
 */
spdxtool_software_sbom_t *
spdxtool_software_sbom_new(pkgconf_client_t *client, const char *spdx_id, const char *creation_id, const char *sbom_type)
{
	if (!client || !spdx_id || !creation_id || !sbom_type)
		return NULL;

	spdxtool_software_sbom_t *sbom = calloc(1, sizeof(spdxtool_software_sbom_t));
	if (!sbom)
		goto err;

	sbom->type = "software_Sbom";
	sbom->spdx_id = strdup(spdx_id);
	sbom->creation_info = strdup(creation_id);
	sbom->sbom_type = strdup(sbom_type);

	if (!sbom->spdx_id || !sbom->creation_info || !sbom->sbom_type)
		goto err;

	return sbom;

err:
	pkgconf_error(client, "spdxtool_software_sbom_new: out of memory");
	spdxtool_software_sbom_free(sbom);
	return NULL;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_software_sbom_free(spdxtool_software_sbom_t *sbom)
 *
 *    Free /Software/Sbom struct
 *
 *    :param spdxtool_software_sbom_t *sbom: Sbom struct to be freed.
 *    :return: nothing
 */
void
spdxtool_software_sbom_free(spdxtool_software_sbom_t *sbom)
{
	if(!sbom)
		return;

	free(sbom->spdx_id);
	free(sbom->creation_info);
	free(sbom->sbom_type);

	free(sbom);
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_software_sbom_to_object(pkgconf_client_t *client, spdxtool_software_sbom_t *sbom)
 *
 *    Serialize /Software/Sbom struct to a JSON value tree. As a side effect,
 *    the package associated with the SBOM's rootElement is registered on the
 *    document via spdxtool_core_spdx_document_add_package, and relationship
 *    element IDs are registered via spdxtool_core_spdx_document_add_element.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param spdxtool_software_sbom_t *sbom: Sbom struct to be serialized.
 *    :return: spdxtool_serialize_value_t * representing the Sbom object.
 */
spdxtool_serialize_value_t *
spdxtool_software_sbom_to_object(pkgconf_client_t *client, spdxtool_software_sbom_t *sbom)
{
	spdxtool_serialize_value_t *ret = NULL;
	spdxtool_serialize_object_list_t *object_list = NULL;
	spdxtool_serialize_array_t *sbom_type_array = NULL;
	spdxtool_serialize_array_t *root_element_array = NULL;
	spdxtool_serialize_array_t *element_array = NULL;
	char *spdx_id = NULL;

	char sep = spdxtool_util_get_uri_separator(client);

	spdx_id = spdxtool_util_tuple_lookup(client, &sbom->rootElement->vars, "spdxId");
	if (!spdx_id)
		goto err;

	object_list = spdxtool_serialize_object_list_new();
	if (!object_list)
		goto err;

	sbom_type_array = spdxtool_serialize_array_new();
	if (!sbom_type_array)
		goto err;

	if (!spdxtool_serialize_array_add_string(sbom_type_array, sbom->sbom_type))
		goto err;

	root_element_array = spdxtool_serialize_array_new();
	if (!root_element_array)
		goto err;

	if (!spdxtool_serialize_array_add_string(root_element_array, spdx_id))
		goto err;

	element_array = spdxtool_serialize_array_new();
	if (!element_array)
		goto err;

	pkgconf_node_t *node = NULL;
	PKGCONF_FOREACH_LIST_ENTRY(sbom->rootElement->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		pkgconf_pkg_t *match = dep->match;
		pkgconf_buffer_t relationship_buf = PKGCONF_BUFFER_INITIALIZER;

		pkgconf_buffer_append_fmt(&relationship_buf, "%s%cdependsOn%c%s", sbom->rootElement->id, sep, sep, match->id);
		char *relationship_str = pkgconf_buffer_freeze(&relationship_buf);
		if (!relationship_str)
			goto err;

		char *spdx_id_relation = spdxtool_util_get_spdx_id_string(client, "Relationship", relationship_str);
		free(relationship_str);
		if (!spdx_id_relation)
			goto err;

		if (!spdxtool_serialize_array_add_string(element_array, spdx_id_relation))
		{
			free(spdx_id_relation);
			goto err;
		}

		if (!spdxtool_core_spdx_document_add_element(client, sbom->spdx_document, spdx_id_relation))
		{
			free(spdx_id_relation);
			goto err;
		}

		free(spdx_id_relation);
	}

	char *value = spdxtool_util_tuple_lookup(client, &sbom->rootElement->vars, "hasDeclaredLicense");
	if (value)
	{
		if (!spdxtool_serialize_array_add_string(element_array, value))
		{
			free(value);
			goto err;
		}

		if (!spdxtool_core_spdx_document_add_element(client, sbom->spdx_document, value))
		{
			free(value);
			goto err;
		}

		free(value);
	}

	value = spdxtool_util_tuple_lookup(client, &sbom->rootElement->vars, "hasConcludedLicense");
	if (value)
	{
		if (!spdxtool_serialize_array_add_string(element_array, value))
		{
			free(value);
			goto err;
		}

		if (!spdxtool_core_spdx_document_add_element(client, sbom->spdx_document, value))
		{
			free(value);
			goto err;
		}

		free(value);
	}

	if (!(spdxtool_serialize_object_add_string(object_list, "type", sbom->type) &&
		spdxtool_serialize_object_add_string(object_list, "creationInfo", sbom->creation_info) &&
		spdxtool_serialize_object_add_string(object_list, "spdxId", sbom->spdx_id)))
	{
		goto err;
	}

	if (!spdxtool_serialize_object_add_array(object_list, "software_sbomType", sbom_type_array))
		goto err;
	sbom_type_array = NULL;

	if (!spdxtool_serialize_object_add_array(object_list, "rootElement", root_element_array))
		goto err;
	root_element_array = NULL;

	if (!spdxtool_serialize_object_add_array(object_list, "element", element_array))
		goto err;
	element_array = NULL;

	if (!spdxtool_core_spdx_document_add_package(client, sbom->spdx_document, sbom->rootElement))
		goto err;

	ret = spdxtool_serialize_value_object(object_list);
	object_list = NULL;

err:
	if (!ret)
		pkgconf_error(client, "spdxtool_software_sbom_to_object: out of memory");

	free(spdx_id);
	spdxtool_serialize_object_list_free(object_list);
	spdxtool_serialize_array_free(sbom_type_array);
	spdxtool_serialize_array_free(root_element_array);
	spdxtool_serialize_array_free(element_array);
	return ret;
}

static bool
serialize_copyright_lines_to_object(spdxtool_serialize_object_list_t *object_list, const pkgconf_list_t *copyright_lines)
{
	pkgconf_buffer_t copyright_buf = PKGCONF_BUFFER_INITIALIZER;
	const pkgconf_node_t *node;

	if (copyright_lines->head == NULL)
		return spdxtool_serialize_object_add_string(object_list, "software_copyrightText", "NOASSERTION") != NULL;

	PKGCONF_FOREACH_LIST_ENTRY(copyright_lines->head, node)
	{
		const pkgconf_bufferset_t *set = node->data;
		pkgconf_buffer_join(&copyright_buf, '\n', pkgconf_buffer_str_or_empty(&set->buffer), NULL);
	}

	bool ok = spdxtool_serialize_object_add_string(object_list, "software_copyrightText", pkgconf_buffer_str_or_empty(&copyright_buf)) != NULL;
	pkgconf_buffer_finalize(&copyright_buf);
	return ok;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_software_package_to_object(pkgconf_client_t *client, pkgconf_pkg_t *pkg, spdxtool_core_spdx_document_t *spdx)
 *
 *    Serialize /Software/Package struct to a JSON value tree. As a side effect,
 *    any license and dependency relationships generated during serialization are
 *    added to the document via spdxtool_core_spdx_document_add_relationship.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param pkgconf_pkg_t *pkg: Package struct to be serialized.
 *    :param spdxtool_core_spdx_document_t *spdx: SpdxDocument to which generated relationships are added.
 *    :return: spdxtool_serialize_value_t * representing the Package object.
 */
spdxtool_serialize_value_t *
spdxtool_software_package_to_object(pkgconf_client_t *client, pkgconf_pkg_t *pkg, spdxtool_core_spdx_document_t *spdx)
{
	spdxtool_serialize_value_t *ret = NULL;
	spdxtool_serialize_object_list_t *object_list = NULL;
	spdxtool_serialize_array_t *originated_by = NULL;
	char *creation_info = NULL;
	char *spdx_id = NULL;
	char *agent = NULL;
	char *spdx_id_license = NULL;
	pkgconf_list_t relations = PKGCONF_LIST_INITIALIZER;
	pkgconf_list_t *cpy_relations = NULL;
	pkgconf_node_t *node = NULL;
	char sep = spdxtool_util_get_uri_separator(client);

	creation_info = spdxtool_util_tuple_lookup(client, &pkg->vars, "creationInfo");
	spdx_id = spdxtool_util_tuple_lookup(client, &pkg->vars, "spdxId");
	agent = spdxtool_util_tuple_lookup(client, &pkg->vars, "agent");

	if (!creation_info || !spdx_id || !agent)
		goto err;

	object_list = spdxtool_serialize_object_list_new();
	if (!object_list)
		goto err;

	originated_by = spdxtool_serialize_array_new();
	if (!originated_by)
		goto err;

	if (!spdxtool_serialize_array_add_string(originated_by, agent))
		goto err;

	if (!(spdxtool_serialize_object_add_string(object_list, "type", "software_Package") &&
		spdxtool_serialize_object_add_string(object_list, "creationInfo", creation_info) &&
		spdxtool_serialize_object_add_string(object_list, "spdxId", spdx_id) &&
		spdxtool_serialize_object_add_string(object_list, "name", pkg->realname)))
	{
		goto err;
	}

	if (!spdxtool_serialize_object_add_array(object_list, "originatedBy", originated_by))
		goto err;
	originated_by = NULL;

	if (!serialize_copyright_lines_to_object(object_list, &pkg->copyright))
		goto err;

	if (!spdxtool_serialize_object_add_string(object_list, "software_homePage",
		pkg->url ? pkg->url : ""))
	{
		goto err;
	}

	if (!spdxtool_serialize_object_add_string(object_list, "software_downloadLocation",
		pkg->source ? pkg->source : ""))
	{
		goto err;
	}

	if (!spdxtool_serialize_object_add_string(object_list, "software_packageVersion", pkg->version))
		goto err;

	PKGCONF_FOREACH_LIST_ENTRY(pkg->license.head, node)
	{
		const pkgconf_license_t *license = node->data;
		if (license->type == PKGCONF_LICENSE_EXPRESSION)
		{
			spdx_id_license = spdxtool_util_get_spdx_id_string(client, "simplelicensing_LicenseExpression", license->data);
			if (!spdx_id_license)
				goto err;

			pkgconf_license_insert(client, &relations, PKGCONF_LICENSE_UNKNOWN, spdx_id_license);
			free(spdx_id_license);
			spdx_id_license = NULL;
		}
	}

	char *tuple_license = spdxtool_util_tuple_lookup(client, &pkg->vars, "hasDeclaredLicense");
	if (tuple_license)
	{
		cpy_relations = calloc(1, sizeof(pkgconf_list_t));
		if (!cpy_relations)
		{
			free(tuple_license);
			goto err;
		}

		pkgconf_license_copy_list(client, cpy_relations, &relations);
		spdxtool_core_relationship_t *relationship = spdxtool_core_relationship_new(client, creation_info, tuple_license, spdx_id, cpy_relations, "hasDeclaredLicense");
		free(tuple_license);
		if (!relationship)
			goto err;
		if (!spdxtool_core_spdx_document_add_relationship(client, spdx, relationship))
			goto err;
		cpy_relations = NULL;
	}

	tuple_license = spdxtool_util_tuple_lookup(client, &pkg->vars, "hasConcludedLicense");
	if (tuple_license)
	{
		cpy_relations = calloc(1, sizeof(pkgconf_list_t));
		if (!cpy_relations)
		{
			free(tuple_license);
			goto err;
		}

		pkgconf_license_copy_list(client, cpy_relations, &relations);
		spdxtool_core_relationship_t *relationship = spdxtool_core_relationship_new(client, creation_info, tuple_license, spdx_id, cpy_relations, "hasConcludedLicense");
		free(tuple_license);
		if (!relationship)
			goto err;
		if (!spdxtool_core_spdx_document_add_relationship(client, spdx, relationship))
			goto err;
		cpy_relations = NULL;
	}
	pkgconf_license_free(&relations);

	PKGCONF_FOREACH_LIST_ENTRY(pkg->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		pkgconf_pkg_t *match = dep->match;
		pkgconf_buffer_t relationship_buf = PKGCONF_BUFFER_INITIALIZER;

		pkgconf_buffer_append_fmt(&relationship_buf, "%s%cdependsOn%c%s", pkg->id, sep, sep, match->id);
		char *relationship_str = pkgconf_buffer_freeze(&relationship_buf);
		if (!relationship_str)
			goto err;

		char *spdx_id_relation = spdxtool_util_get_spdx_id_string(client, "Relationship", relationship_str);
		free(relationship_str);
		if (!spdx_id_relation)
			goto err;

		char *spdx_id_package = spdxtool_util_get_spdx_id_string(client, "Package", match->id);
		if (!spdx_id_package)
		{
			free(spdx_id_relation);
			goto err;
		}

		cpy_relations = calloc(1, sizeof(pkgconf_list_t));
		if (!cpy_relations)
		{
			free(spdx_id_relation);
			free(spdx_id_package);
			goto err;
		}

		pkgconf_license_insert(client, cpy_relations, PKGCONF_LICENSE_UNKNOWN, spdx_id_package);
		spdxtool_core_relationship_t *relationship = spdxtool_core_relationship_new(client, creation_info, spdx_id_relation, spdx_id, cpy_relations, "dependsOn");
		free(spdx_id_relation);
		free(spdx_id_package);
		if (!relationship)
			goto err;
		if (!spdxtool_core_spdx_document_add_relationship(client, spdx, relationship))
			goto err;
		cpy_relations = NULL;
	}

	ret = spdxtool_serialize_value_object(object_list);
	object_list = NULL;

err:
	if (!ret)
		pkgconf_error(client, "spdxtool_software_package_to_object: out of memory");

	free(creation_info);
	free(spdx_id);
	free(agent);
	free(spdx_id_license);
	spdxtool_serialize_object_list_free(object_list);
	spdxtool_serialize_array_free(originated_by);
	return ret;
}
