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
#include "util.h"
#include "serialize.h"
#include "software.h"
#include "core.h"

/*
 * !doc
 *
 * .. c:function:: spdxtool_software_sbom_t *spdxtool_software_sbom_new(pkgconf_client_t *client, char *spdx_id, char *creation_id, char *sbom_type)
 *
 *    Create new /Software/Sbom struct
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param char *spdx_id: spdxId for this SBOM element
 *    :param char *creation_id: id for CreationInfo
 *    :param char *sbom_type: Sbom types can be found SPDX documention
 *    :return: NULL if some problem occurs and Sbom struct if not
 */
spdxtool_software_sbom_t *
spdxtool_software_sbom_new(pkgconf_client_t *client, char *spdx_id, char *creation_id, char *sbom_type)
{
	spdxtool_software_sbom_t *sbom_struct = NULL;

	if(!client || !spdx_id || !creation_id || !sbom_type)
	{
		return NULL;
	}

	(void)client;

	sbom_struct = calloc(1, sizeof(spdxtool_software_sbom_t));

	if(!sbom_struct)
	{
		pkgconf_error(client, "Memory exhausted! Can't create sbom struct.");
		return NULL;
	}

	sbom_struct->type="software_Sbom";
	sbom_struct->spdx_id = spdx_id;
	sbom_struct->creation_info = creation_id;
	sbom_struct->sbom_type = sbom_type;
	return sbom_struct;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_software_sbom_free(spdxtool_software_sbom_t *sbom_struct)
 *
 *    Free /Software/Sbom struct
 *
 *    :param spdxtool_software_sbom_t *sbom_struct: Sbom struct to be freed.
 *    :return: nothing
 */
void
spdxtool_software_sbom_free(spdxtool_software_sbom_t *sbom_struct)
{

	if(!sbom_struct)
	{
		return;
	}

	if(sbom_struct->spdx_id)
	{
		free(sbom_struct->spdx_id);
		sbom_struct->spdx_id = NULL;
	}

	if(sbom_struct->creation_info)
	{
		free(sbom_struct->creation_info);
		sbom_struct->creation_info = NULL;
	}

	if(sbom_struct->sbom_type)
	{
		free(sbom_struct->sbom_type);
		sbom_struct->sbom_type = NULL;
	}

	free(sbom_struct);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_software_sbom_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_software_sbom_t *sbom_struct, bool last)
 *
 *    Serialize /Software/Sbom struct to JSON
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param pkgconf_buffer_t *buffer: Buffer where struct is serialized
 *    :param spdxtool_software_sbom_t *sbom_struct: SimpleLicensingText struct to be serialized
 *    :param bool last: Is this last CreationInfo struct or does it need comma at the end. True comma False not
 *    :return: nothing
 */
void
spdxtool_software_sbom_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_software_sbom_t *sbom_struct, bool last)
{
	pkgconf_node_t *node = NULL;
	char *value;

	spdxtool_serialize_obj_start(buffer, 2);
	spdxtool_serialize_parm_and_string(buffer, "@type", sbom_struct->type, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "creationInfo", sbom_struct->creation_info, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "spdxId", sbom_struct->spdx_id, 3, true);
	spdxtool_serialize_parm_and_char(buffer, "software_sbomType", '[', 3, false);
	spdxtool_serialize_string(buffer, sbom_struct->sbom_type, 4, false);
	spdxtool_serialize_array_end(buffer, 3, true);
	spdxtool_serialize_parm_and_char(buffer, "rootElement", '[', 3, false);
	spdxtool_serialize_string(buffer, pkgconf_tuple_find(client, &sbom_struct->rootElement->vars, "spdxId"), 4, false);
	spdxtool_serialize_array_end(buffer, 3, true);
	spdxtool_serialize_parm_and_char(buffer, "element", '[', 3, false);
	PKGCONF_FOREACH_LIST_ENTRY(sbom_struct->rootElement->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		pkgconf_pkg_t *match = dep->match;
		char *spdx_id_relation = NULL;
		char tmp_str[1024];

		// New relation spdxId
		snprintf(tmp_str, 1024, "%s/dependsOn/%s", sbom_struct->rootElement->realname, match->realname);

		spdx_id_relation = spdxtool_util_get_spdx_id_string(client, "Releationship", tmp_str);

		spdxtool_serialize_string(buffer, spdx_id_relation, 4, true);

		spdxtool_core_spdx_document_add_element(client, sbom_struct->spdx_document, strdup(spdx_id_relation));

		free(spdx_id_relation);
	}

	value = pkgconf_tuple_find(client, &sbom_struct->rootElement->vars, "hasDeclaredLicense");
	if (value != NULL)
	{
		spdxtool_serialize_string(buffer, value, 4, true);
		spdxtool_core_spdx_document_add_element(client, sbom_struct->spdx_document, strdup(value));
	}

	value = pkgconf_tuple_find(client, &sbom_struct->rootElement->vars, "hasConcludedLicense");
	if (value != NULL)
	{
		spdxtool_serialize_string(buffer, value, 4, false);
		spdxtool_core_spdx_document_add_element(client, sbom_struct->spdx_document, strdup(value));
	}

	spdxtool_serialize_array_end(buffer, 3, false);
	spdxtool_serialize_obj_end(buffer, 2, last);

	spdxtool_software_package_serialize(client, buffer, sbom_struct->rootElement, false);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_software_sbom_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_software_sbom_t *sbom_struct, bool last)
 *
 *    Serialize /Software/Package struct to JSON
 *    This is bit special case as it takes pkgconf pkg struct and serializes that one
 *    There is not new or free for this stype
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param pkgconf_buffer_t *buffer: Buffer where struct is serialized
 *    :param pkgconf_pkg_t *pkg: Pkgconf package
 *    :param bool last: Is this last CreationInfo struct or does it need comma at the end. True comma False not
 *    :return: nothing
 */
void
spdxtool_software_package_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, pkgconf_pkg_t *pkg, bool last)
{
	spdxtool_core_relationship_t *relationship_struct = NULL;
	char *spdx_id_license = NULL, *tuple_license;
	pkgconf_node_t *node = NULL;

	(void) last;

	spdxtool_serialize_obj_start(buffer, 2);
	spdxtool_serialize_parm_and_string(buffer, "@type", "software_Package", 3, true);
	spdxtool_serialize_parm_and_string(buffer, "creationInfo", pkgconf_tuple_find(client, &pkg->vars, "creationInfo"), 3, true);
	spdxtool_serialize_parm_and_string(buffer, "spdxId", pkgconf_tuple_find(client, &pkg->vars, "spdxId"), 3, true);
	spdxtool_serialize_parm_and_string(buffer, "name", pkg->realname, 3, true);
	spdxtool_serialize_parm_and_char(buffer, "originatedBy", '[', 3, false);
	spdxtool_serialize_string(buffer, pkgconf_tuple_find(client, &pkg->vars, "agent"), 4, false);
	spdxtool_serialize_array_end(buffer, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "software_copyrightText", "NOASSERTION", 3, true);
	spdxtool_serialize_parm_and_string(buffer, "software_packageVersion", pkg->version, 3, false);
	spdxtool_serialize_obj_end(buffer, 2, true);

	/* These are must so add them right a way */
	spdx_id_license = spdxtool_util_get_spdx_id_string(client, "simplelicensing_LicenseExpression", pkg->license);

	tuple_license = pkgconf_tuple_find(client, &pkg->vars, "hasDeclaredLicense");
	if (tuple_license != NULL)
	{
		relationship_struct = spdxtool_core_relationship_new(client, strdup(pkgconf_tuple_find(client, &pkg->vars, "creationInfo")), strdup(tuple_license), strdup(pkgconf_tuple_find(client, &pkg->vars, "spdxId")), strdup(spdx_id_license), strdup("hasDeclaredLicense"));
		spdxtool_core_relationship_serialize(client, buffer, relationship_struct, true);
		spdxtool_core_relationship_free(relationship_struct);
	}

	tuple_license = pkgconf_tuple_find(client, &pkg->vars, "hasConcludedLicense");
	if (tuple_license != NULL)
	{
		relationship_struct = spdxtool_core_relationship_new(client, strdup(pkgconf_tuple_find(client, &pkg->vars, "creationInfo")), strdup(tuple_license), strdup(pkgconf_tuple_find(client, &pkg->vars, "spdxId")), spdx_id_license, strdup("hasConcludedLicense"));
		spdxtool_core_relationship_serialize(client, buffer, relationship_struct, true);
		spdxtool_core_relationship_free(relationship_struct);
	}

	PKGCONF_FOREACH_LIST_ENTRY(pkg->required.head, node)
	{
		pkgconf_dependency_t *dep = node->data;
		pkgconf_pkg_t *match = dep->match;
		char *spdx_id_relation = NULL;
		char *spdx_id_package = NULL;
		char tmp_str[1024];

		// New relation spdxId
		snprintf(tmp_str, 1024, "%s/dependsOn/%s", pkg->realname, match->realname);
		spdx_id_relation = spdxtool_util_get_spdx_id_string(client, "Releationship", tmp_str);

		// new package spdxId which will be hopefully come later on
		spdx_id_package = spdxtool_util_get_spdx_id_string(client, "Package", match->realname);

		relationship_struct = spdxtool_core_relationship_new(client, strdup(pkgconf_tuple_find(client, &pkg->vars, "creationInfo")), spdx_id_relation, strdup(pkgconf_tuple_find(client, &pkg->vars, "spdxId")), spdx_id_package, strdup("dependsOn"));
		spdxtool_core_relationship_serialize(client, buffer, relationship_struct, true);
		spdxtool_core_relationship_free(relationship_struct);
	}
}
