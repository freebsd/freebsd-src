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
#include "core.h"
#include "software.h"
#include "simplelicensing.h"

/*
 * !doc
 *
 * .. c:function:: spdxtool_core_agent_t *spdxtool_core_agent_new(pkgconf_client_t *client, const char *creation_info_id, const char *name)
 *
 *    Create new /Core/Agent struct
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param char *creation_info_id: CreationInfo spdxId
 *    :param char *name: Name of agent
 *    :return: NULL if some problem occurs and Agent struct if not
 */
spdxtool_core_agent_t *
spdxtool_core_agent_new(pkgconf_client_t *client, const char *creation_info_id, const char *name)
{
	if (!client || !creation_info_id || !name)
		return NULL;

	spdxtool_core_agent_t *agent = calloc(1, sizeof(spdxtool_core_agent_t));
	if (!agent)
		goto err;

	agent->type = "Agent";

	char *spdx_id_name = strdup(name);
	if (!spdx_id_name)
		goto err;

	spdxtool_util_string_correction(spdx_id_name);

	agent->spdx_id = spdxtool_util_get_spdx_id_string(client, agent->type, spdx_id_name);
	free(spdx_id_name);

	agent->creation_info = strdup(creation_info_id);
	agent->name = strdup(name);

	if (!agent->spdx_id || !agent->creation_info || !agent->name)
		goto err;

	return agent;

err:
	pkgconf_error(client, "spdxtool_core_agent_new: out of memory");
	spdxtool_core_agent_free(agent);
	return NULL;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_agent_free(spdxtool_core_agent_t *agent)
 *
 *    Free /Core/Agent struct
 *
 *    :param spdxtool_core_agent_t *agent: Agent struct to be freed.
 *    :return: nothing
 */
void
spdxtool_core_agent_free(spdxtool_core_agent_t *agent)
{
	if (!agent)
		return;

	free(agent->creation_info);
	free(agent->spdx_id);
	free(agent->name);

	free(agent);
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_core_agent_to_object(pkgconf_client_t *client, const spdxtool_core_agent_t *agent)
 *
 *    Serialize /Core/Agent struct to a JSON value tree.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param const spdxtool_core_agent_t *agent: Agent struct to be serialized.
 *    :return: spdxtool_serialize_value_t * representing the Agent object.
 */
spdxtool_serialize_value_t *
spdxtool_core_agent_to_object(pkgconf_client_t *client, const spdxtool_core_agent_t *agent)
{
	spdxtool_serialize_value_t *ret = NULL;
	spdxtool_serialize_object_list_t *object_list = spdxtool_serialize_object_list_new();
	if (!object_list)
		goto err;

	if (!(spdxtool_serialize_object_add_string(object_list, "type", agent->type) &&
		spdxtool_serialize_object_add_string(object_list, "creationInfo", agent->creation_info) &&
		spdxtool_serialize_object_add_string(object_list, "spdxId", agent->spdx_id) &&
		spdxtool_serialize_object_add_string(object_list, "name", agent->name)))
	{
		goto err;
	}

	ret = spdxtool_serialize_value_object(object_list);
	object_list = NULL;

err:
	if (!ret)
		pkgconf_error(client, "spdxtool_core_agent_to_object: out of memory");

	spdxtool_serialize_object_list_free(object_list);
	return ret;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_core_creation_info_t *spdxtool_core_creation_info_new(pkgconf_client_t *client, const char *agent_id, const char *id, const char *time)
 *
 *    Create new /Core/CreationInfo struct
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param const char *agent_id: Agent spdxId
 *    :param const char *id: Id for creation info
 *    :param const char *time: If NULL current time is used if not then
 *                             this time string is used. Time string should be
 *                             in ISO8601 format: YYYY-MM-DDTHH:MM:SSZ
 *    :return: NULL if some problem occurs and CreationInfo struct if not
 */
spdxtool_core_creation_info_t *
spdxtool_core_creation_info_new(pkgconf_client_t *client, const char *agent_id, const char *id, const char *time)
{
	if (!client || !agent_id || !id)
		return NULL;

	spdxtool_core_creation_info_t *creation = calloc(1, sizeof(spdxtool_core_creation_info_t));
	if (!creation)
		goto err;

	creation->type = "CreationInfo";
	creation->created_using = "pkgconf spdxtool";
	creation->id = strdup(id);
	creation->created = time ? strdup(time) : spdxtool_util_get_current_iso8601_time();
	creation->created_by = strdup(agent_id);
	creation->spec_version = strdup(spdxtool_util_get_spdx_version(client));

	if (!creation->id || !creation->created || !creation->created_by || !creation->spec_version)
		goto err;

	return creation;

err:
	pkgconf_error(client, "spdxtool_core_creation_info_new: out of memory");
	spdxtool_core_creation_info_free(creation);
	return NULL;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_creation_info_free(spdxtool_core_creation_info_t *creation)
 *
 *    Free /Core/CreationInfo struct
 *
 *    :param spdxtool_core_creation_info_t *creation: CreationInfo struct to be freed.
 *    :return: nothing
 */
void
spdxtool_core_creation_info_free(spdxtool_core_creation_info_t *creation)
{
	if (!creation)
		return;

	free(creation->id);
	free(creation->created);
	free(creation->created_by);
	free(creation->spec_version);

	free(creation);
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_core_creation_info_to_object(pkgconf_client_t *client, const spdxtool_core_creation_info_t *creation)
 *
 *    Serialize /Core/CreationInfo struct to a JSON value tree.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param const spdxtool_core_creation_info_t *creation: CreationInfo struct to be serialized.
 *    :return: spdxtool_serialize_value_t * representing the CreationInfo object.
 */
spdxtool_serialize_value_t *
spdxtool_core_creation_info_to_object(pkgconf_client_t *client, const spdxtool_core_creation_info_t *creation)
{
	spdxtool_serialize_value_t *ret = NULL;
	spdxtool_serialize_object_list_t *object_list = spdxtool_serialize_object_list_new();
	if (!object_list)
		goto err;

	spdxtool_serialize_array_t *created_by = spdxtool_serialize_array_new();
	if (!created_by)
		goto err;

	if (!spdxtool_serialize_array_add_string(created_by, creation->created_by))
	{
		spdxtool_serialize_array_free(created_by);
		goto err;
	}

	if (!(spdxtool_serialize_object_add_string(object_list, "type", creation->type) &&
		spdxtool_serialize_object_add_string(object_list, "@id", creation->id) &&
		spdxtool_serialize_object_add_string(object_list, "created", creation->created) &&
		spdxtool_serialize_object_add_array(object_list, "createdBy", created_by) &&
		spdxtool_serialize_object_add_string(object_list, "specVersion", creation->spec_version)))
	{
		goto err;
	}

	ret = spdxtool_serialize_value_object(object_list);
	object_list = NULL;

err:
	if (!ret)
		pkgconf_error(client, "spdxtool_core_creation_info_to_object: out of memory");

	spdxtool_serialize_object_list_free(object_list);
	return ret;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_core_creation_info_t *spdxtool_core_creation_info_new(pkgconf_client_t *client, const char *spdx_id, const char *creation_id, const char *agent)
 *
 *    Create new /Core/SpdxDocument struct
 *    In SPDX Lite SBOM there can be only one SpdxDocument
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param const char *spdx_id: Id of this SpdxDocument
 *    :param const char *creation_id: Id for creation info
 *    :param const char *agent_id: Agent for this document
 *    :return: NULL if some problem occurs and SpdxDocument struct if not
 */
spdxtool_core_spdx_document_t *
spdxtool_core_spdx_document_new(pkgconf_client_t *client, const char *spdx_id, const char *creation_id, const char *agent_id)
{
	if (!client || !spdx_id || !creation_id || !agent_id)
		return NULL;

	spdxtool_core_spdx_document_t *spdx = calloc(1, sizeof(spdxtool_core_spdx_document_t));
	if (!spdx)
		goto err;

	spdx->type = "SpdxDocument";
	spdx->spdx_id = strdup(spdx_id);
	spdx->agent = strdup(agent_id);
	spdx->creation_info = strdup(creation_id);

	if (!spdx->spdx_id || !spdx->agent || !spdx->creation_info)
		goto err;

	return spdx;

err:
	pkgconf_error(client, "spdxtool_core_spdx_document_new: out of memory");
	spdxtool_core_spdx_document_free(spdx);
	return NULL;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_spdx_document_free(spdxtool_core_spdx_document_t *spdx)
 *
 *    Free /Core/SpdxDocument struct
 *
 *    :param spdxtool_core_spdx_document_t *spdx: SpdxDocument struct to be freed.
 *    :return: nothing
 */
void
spdxtool_core_spdx_document_free(spdxtool_core_spdx_document_t *spdx)
{
	pkgconf_node_t *iter = NULL, *iter_next = NULL;

	if (!spdx)
		return;

	free(spdx->spdx_id);
	free(spdx->creation_info);
	free(spdx->agent);

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(spdx->rootElement.head, iter_next, iter)
	{
		spdxtool_software_sbom_t *sbom = iter->data;
		spdxtool_software_sbom_free(sbom);
		free(iter);
	}

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(spdx->element.head, iter_next, iter)
	{
		free(iter->data);
		free(iter);
	}

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(spdx->licenses.head, iter_next, iter)
	{
		spdxtool_simplelicensing_license_expression_t *expression = iter->data;
		spdxtool_simplelicensing_licenseExpression_free(expression);
		free(iter);
	}

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(spdx->relationships.head, iter_next, iter)
	{
		spdxtool_core_relationship_t *relationship = iter->data;
		spdxtool_core_relationship_free(relationship);
		free(iter);
	}

	PKGCONF_FOREACH_LIST_ENTRY_SAFE(spdx->packages.head, iter_next, iter)
	{
		free(iter);
	}

	free(spdx);
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_core_spdx_document_to_object(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx)
 *
 *    Serialize /Core/SpdxDocument struct to a JSON value tree. This function
 *    should be called after all SBOMs and packages have been serialized so that
 *    the document's element and rootElement lists are fully populated.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param spdxtool_core_spdx_document_t *spdx: SpdxDocument struct to be serialized.
 *    :return: spdxtool_serialize_value_t * representing the SpdxDocument object.
 */
spdxtool_serialize_value_t *
spdxtool_core_spdx_document_to_object(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx)
{
	spdxtool_serialize_value_t *ret = NULL;
	spdxtool_serialize_object_list_t *object_list = NULL;
	spdxtool_serialize_array_t *root_element_array = NULL;
	spdxtool_serialize_array_t *element_array = NULL;

	object_list = spdxtool_serialize_object_list_new();
	if (!object_list)
		goto err;

	root_element_array = spdxtool_serialize_array_new();
	if (!root_element_array)
		goto err;

	pkgconf_node_t *iter = NULL;
	PKGCONF_FOREACH_LIST_ENTRY(spdx->rootElement.head, iter)
	{
		spdxtool_software_sbom_t *sbom = iter->data;
		if (!spdxtool_serialize_array_add_string(root_element_array, sbom->spdx_id))
			goto err;
	}

	element_array = spdxtool_serialize_array_new();
	if (!element_array)
		goto err;

	if (!spdxtool_serialize_array_add_string(element_array, spdx->agent))
		goto err;

	PKGCONF_FOREACH_LIST_ENTRY(spdx->element.head, iter)
	{
		char *element_id = iter->data;
		if (!spdxtool_serialize_array_add_string(element_array, element_id))
			goto err;
	}

	PKGCONF_FOREACH_LIST_ENTRY(spdx->rootElement.head, iter)
	{
		spdxtool_software_sbom_t *sbom = iter->data;
		char *pkg_spdx_id = spdxtool_util_tuple_lookup(client, &sbom->rootElement->vars, "spdxId");
		if (!pkg_spdx_id)
			goto err;

		bool ok = spdxtool_serialize_array_add_string(element_array, sbom->spdx_id) &&
			spdxtool_serialize_array_add_string(element_array, pkg_spdx_id);
		free(pkg_spdx_id);

		if (!ok)
			goto err;
	}

	if (!(spdxtool_serialize_object_add_string(object_list, "type", spdx->type) &&
		spdxtool_serialize_object_add_string(object_list, "creationInfo", spdx->creation_info) &&
		spdxtool_serialize_object_add_string(object_list, "spdxId", spdx->spdx_id) &&
		spdxtool_serialize_object_add_array(object_list, "rootElement", root_element_array) &&
		spdxtool_serialize_object_add_array(object_list, "element", element_array)))
	{
		goto err;
	}

	root_element_array = NULL;
	element_array = NULL;

	ret = spdxtool_serialize_value_object(object_list);
	object_list = NULL;

err:
	if (!ret)
		pkgconf_error(client, "spdxtool_core_spdx_document_to_object: out of memory");

	spdxtool_serialize_object_list_free(object_list);
	spdxtool_serialize_array_free(root_element_array);
	spdxtool_serialize_array_free(element_array);
	return ret;
}

/*
 * !doc
 *
 * .. c:function:: bool spdxtool_core_spdx_document_is_license(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, const char *license)
 *
 *    Find out if specific license is already there.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param spdxtool_core_spdx_document_t *spdx: SpdxDocument struct being used.
 *    :param const char *license: SPDX name of license
 *    :return: true is license is there and false if not
 */
bool
spdxtool_core_spdx_document_is_license(pkgconf_client_t *client, const spdxtool_core_spdx_document_t *spdx, const char *license)
{
	pkgconf_node_t *iter = NULL;
	spdxtool_simplelicensing_license_expression_t *expression = NULL;

	(void) client;

	if (!license || !spdx)
	{
		return false;
	}

	PKGCONF_FOREACH_LIST_ENTRY(spdx->licenses.head, iter)
	{
		expression = iter->data;
		if (!strcmp(expression->license_expression, license))
		{
			return true;
		}
	}

	return false;
}

/*
 * !doc
 *
 * .. c:function:: bool spdxtool_core_spdx_document_add_license(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, const char *license)
 *
 *    Add license to SpdxDocument and make sure that specific license is not already there.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param spdxtool_core_spdx_document_t *spdx: SpdxDocument struct being used.
 *    :param const char *license: SPDX name of license
 *    :return: true on success, false on failure
 */
bool
spdxtool_core_spdx_document_add_license(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, const char *license)
{
	if (!license || !spdx)
		return false;

	if (spdxtool_core_spdx_document_is_license(client, spdx, license))
		return true;

	pkgconf_node_t *node = calloc(1, sizeof(pkgconf_node_t));
	if (!node)
	{
		pkgconf_error(client, "spdxtool_core_spdx_document_add_license: out of memory");
		return false;
	}

	spdxtool_simplelicensing_license_expression_t *expression = spdxtool_simplelicensing_licenseExpression_new(client, license);
	if (!expression)
	{
		free(node);
		return false;
	}

	pkgconf_node_insert_tail(node, expression, &spdx->licenses);
	if (!spdxtool_core_spdx_document_add_element(client, spdx, expression->spdx_id))
		return false;

	return true;
}

/*
 * !doc
 *
 * .. c:function:: bool spdxtool_core_spdx_document_add_element(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, const char *element)
 *
 *    Add element spdxId to SpdxDocument
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param spdxtool_core_spdx_document_t *spdx: SpdxDocument struct being used.
 *    :param char *element: spdxId of element
 *    :return: true on success, false on failure
 */
bool
spdxtool_core_spdx_document_add_element(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, const char *element)
{
	if (!element || !spdx)
		return false;

	pkgconf_node_t *node = calloc(1, sizeof(pkgconf_node_t));
	if (!node)
	{
		pkgconf_error(client, "spdxtool_core_spdx_document_add_element: out of memory");
		return false;
	}

	char *nelement = strdup(element);
	if (!nelement)
	{
		pkgconf_error(client, "spdxtool_core_spdx_document_add_element: out of memory");
		free(node);
		return false;
	}

	pkgconf_node_insert_tail(node, nelement, &spdx->element);
	return true;
}

/*
 * !doc
 *
 * .. c:function:: bool spdxtool_core_spdx_document_add_relationship(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, spdxtool_core_relationship_t *relationship)
 *
 *    Add relationship rel to SpdxDocument
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param spdxtool_core_spdx_document_t *spdx: SpdxDocument struct being used.
 *    :param spdxtool_core_relationship_t *relationship: relationship to add.
 *    :return: true on success, false on failure
 */
bool
spdxtool_core_spdx_document_add_relationship(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, spdxtool_core_relationship_t *relationship)
{
	if (!client || !spdx || !relationship)
		return false;

	pkgconf_node_t *node = calloc(1, sizeof(pkgconf_node_t));
	if (!node)
	{
		pkgconf_error(client, "spdxtool_core_spdx_document_add_relationship: out of memory");
		return false;
	}

	pkgconf_node_insert_tail(node, relationship, &spdx->relationships);
	return true;
}

/*
 * !doc
 *
 * .. c:function:: bool spdxtool_core_spdx_document_add_package(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, pkgconf_pkg_t *pkg)
 *
 *    Register a package with the SpdxDocument for later serialization. The document
 *    does not take ownership of the package pointer; the package must outlive the
 *    document and will not be freed by spdxtool_core_spdx_document_free.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param spdxtool_core_spdx_document_t *spdx: SpdxDocument struct to register the package with.
 *    :param pkgconf_pkg_t *pkg: Package to register. Ownership is NOT transferred.
 *    :return: true on success, false on failure
 */
bool
spdxtool_core_spdx_document_add_package(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx, pkgconf_pkg_t *pkg)
{
	if (!client || !spdx || !pkg)
		return false;

	pkgconf_node_t *node = calloc(1, sizeof(pkgconf_node_t));
	if (!node)
	{
		pkgconf_error(client, "spdxtool_core_spdx_document_add_package: out of memory");
		return false;
	}

	pkgconf_node_insert_tail(node, pkg, &spdx->packages);
	return true;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_core_relationship_t *spdxtool_core_relationship_new(pkgconf_client_t *client, const char *creation_info_id, const char *spdx_id, const char *from, const char *to, const char *relationship_type)
 *
 *    Create new /Core/Relationship struct
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param const char *creation_id: Id for creation info
 *    :param const char *spdx_id: Id of this SpdxDocument
 *    :param const char *from: from spdxId
 *    :param const char *to: to spdxId
 *    :param const char *relationship_type: These can be found on SPDX documentation
 *    :return: NULL if some problem occurs and SpdxDocument struct if not
 */
spdxtool_core_relationship_t *
spdxtool_core_relationship_new(pkgconf_client_t *client, const char *creation_info_id, const char *spdx_id, const char *from, pkgconf_list_t *to, const char *relationship_type)
{
	if (!client || !creation_info_id || !spdx_id || !from || !to || !relationship_type)
		return NULL;

	spdxtool_core_relationship_t *relationship = calloc(1, sizeof(spdxtool_core_relationship_t));
	if (!relationship)
		goto err;

	relationship->type = "Relationship";
	relationship->creation_info = strdup(creation_info_id);
	relationship->spdx_id = strdup(spdx_id);
	relationship->from = strdup(from);
	relationship->to = to;
	relationship->relationship_type = strdup(relationship_type);

	if (!relationship->creation_info || !relationship->spdx_id || !relationship->from || !relationship->relationship_type)
		goto err;

	return relationship;

err:
	pkgconf_error(client, "spdxtool_core_relationship_new: out of memory");
	spdxtool_core_relationship_free(relationship);
	return NULL;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_relationship_free(spdxtool_core_relationship_t *relationship)
 *
 *    Free /Core/Relationship struct
 *
 *    :param spdxtool_core_relationship_t *relationship: Relationship struct to be freed.
 *    :return: nothing
 */
void
spdxtool_core_relationship_free(spdxtool_core_relationship_t *relationship)
{
	if (!relationship)
		return;

	free(relationship->spdx_id);
	free(relationship->creation_info);
	free(relationship->from);
	pkgconf_license_free(relationship->to);
	free(relationship->to);
	free(relationship->relationship_type);

	free(relationship);
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_core_relationship_to_object(pkgconf_client_t *client, const spdxtool_core_relationship_t *relationship)
 *
 *    Serialize /Core/Relationship struct to a JSON value tree.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param const spdxtool_core_relationship_t *relationship: Relationship struct to be serialized.
 *    :return: spdxtool_serialize_value_t * representing the Relationship object.
 */
spdxtool_serialize_value_t *
spdxtool_core_relationship_to_object(pkgconf_client_t *client, const spdxtool_core_relationship_t *relationship)
{
	spdxtool_serialize_value_t *ret = NULL;
	spdxtool_serialize_object_list_t *object_list = spdxtool_serialize_object_list_new();
	if (!object_list)
		goto err;

	spdxtool_serialize_array_t *to = spdxtool_serialize_array_new();
	if (!to)
		goto err;

	pkgconf_node_t *node = NULL;
	PKGCONF_FOREACH_LIST_ENTRY(relationship->to->head, node)
	{
		const pkgconf_license_t *license = node->data;
		if (!spdxtool_serialize_array_add_string(to, license->data))
		{
			spdxtool_serialize_array_free(to);
			goto err;
		}
	}

	if (!(spdxtool_serialize_object_add_string(object_list, "type", relationship->type) &&
		spdxtool_serialize_object_add_string(object_list, "creationInfo", relationship->creation_info) &&
		spdxtool_serialize_object_add_string(object_list, "spdxId", relationship->spdx_id) &&
		spdxtool_serialize_object_add_string(object_list, "from", relationship->from) &&
		spdxtool_serialize_object_add_array(object_list, "to", to) &&
		spdxtool_serialize_object_add_string(object_list, "relationshipType", relationship->relationship_type)))
	{
		goto err;
	}

	ret = spdxtool_serialize_value_object(object_list);
	object_list = NULL;

err:
	if (!ret)
		pkgconf_error(client, "spdxtool_core_relationship_to_object: out of memory");

	spdxtool_serialize_object_list_free(object_list);
	return ret;
}
