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
 * .. c:function:: spdxtool_core_agent_t *spdxtool_core_agent_new(pkgconf_client_t *client, char *creation_info_id, char *name)
 *
 *    Create new /Core/Agent struct
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param char *creation_info_id: CreationInfo spdxId
 *    :param char *name: Name of agent
 *    :return: NULL if some problem occurs and Agent struct if not
 */
spdxtool_core_agent_t *
spdxtool_core_agent_new(pkgconf_client_t *client, char *creation_info_id, char *name)
{
	char spdx_id_name[1024];
	char *spdx_id = NULL;
	spdxtool_core_agent_t *agent_struct = NULL;

	if(!client || !creation_info_id || !name)
	{
		return NULL;
	}

	agent_struct = calloc(1, sizeof(spdxtool_core_agent_t));

	if(!agent_struct)
	{
		pkgconf_error(client, "Memory exhausted! Can't create agent struct.");
		return NULL;
	}

	agent_struct->type = "Agent";
	memset(spdx_id_name, 0x00, 1024);
	strncpy(spdx_id_name, name, 1023);
	spdxtool_util_string_correction(spdx_id_name);
	spdx_id = spdxtool_util_get_spdx_id_string(client, agent_struct->type, spdx_id_name);

	agent_struct->creation_info = creation_info_id;
	agent_struct->spdx_id = spdx_id;
	agent_struct->name = name;
	return agent_struct;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_agent_free(spdxtool_core_agent_t *agent_struct)
 *
 *    Free /Core/Agent struct
 *
 *    :param spdxtool_core_agent_t *agent_struct: Agent struct to be freed.
 *    :return: nothing
 */
void
spdxtool_core_agent_free(spdxtool_core_agent_t *agent_struct)
{
	if(!agent_struct)
	{
		return;
	}

	if(agent_struct->creation_info)
	{
		free(agent_struct->creation_info);
		agent_struct->creation_info = NULL;
	}

	if(agent_struct->spdx_id)
	{
		free(agent_struct->spdx_id);
		agent_struct->spdx_id = NULL;
	}

	if(agent_struct->name)
	{
		free(agent_struct->name);
		agent_struct->name = NULL;
	}

	free(agent_struct);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_agent_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_agent_t *agent_struct, bool last)
 *
 *    Serialize /Core/Agent struct to JSON
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param pkgconf_buffer_t *buffer: Buffer where struct is serialized
 *    :param spdxtool_core_agent_t *agent_struct: Agent struct to be serialized
 *    :param bool last: Is this last Agent struct or does it need comma at the end. True comma False not
 *    :return: nothing
 */
void
spdxtool_core_agent_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_agent_t *agent_struct, bool last)
{
	(void) client;
	spdxtool_serialize_obj_start(buffer, 2);
	spdxtool_serialize_parm_and_string(buffer, "@type", agent_struct->type, 3, 1);
	spdxtool_serialize_parm_and_string(buffer, "creationInfo", agent_struct->creation_info, 3, 1);
	spdxtool_serialize_parm_and_string(buffer, "spdxId", agent_struct->spdx_id, 3, 1);
	spdxtool_serialize_parm_and_string(buffer, "name", agent_struct->name, 3, 0);
	spdxtool_serialize_obj_end(buffer, 2, last);
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_core_creation_info_t *spdxtool_core_creation_info_new(pkgconf_client_t *client, char *agent_id, char *id)
 *
 *    Create new /Core/CreationInfo struct
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param char *agent_id: Agent spdxId
 *    :param char *id: Id for creation info
 *    :param char *time: If NULL current time is used if not then
 *                       this time string is used. Time string should be
 *                       in ISO8601 format: YYYY-MM-DDTHH:MM:SSZ
 *    :return: NULL if some problem occurs and CreationInfo struct if not
 */
spdxtool_core_creation_info_t *
spdxtool_core_creation_info_new(pkgconf_client_t *client, char *agent_id, char *id, char *time)
{
	spdxtool_core_creation_info_t *creation_struct = NULL;

	if(!client || !agent_id || !id)
	{
		return NULL;
	}

	creation_struct = calloc(1, sizeof(spdxtool_core_creation_info_t));
	if(!creation_struct)
	{
		pkgconf_error(client, "Memory exhausted! Can't create agent struct.");
		return NULL;
	}

	creation_struct->type = "CreationInfo";
	creation_struct->id = id;
	if(!time)
	{
		creation_struct->created = spdxtool_util_get_current_iso8601_time();
	}
	else
	{
		creation_struct->created = time;
	}
	creation_struct->created_by = agent_id;
	creation_struct->created_using="pkgconf spdxtool";
	creation_struct->spec_version = spdxtool_util_get_spdx_version(client);
	return creation_struct;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_creation_info_free(spdxtool_core_creation_info_t *creation_struct)
 *
 *    Free /Core/CreationInfo struct
 *
 *    :param spdxtool_core_creation_info_t *creation_struct: CreationInfo struct to be freed.
 *    :return: nothing
 */
void
spdxtool_core_creation_info_free(spdxtool_core_creation_info_t *creation_struct)
{
	if(!creation_struct)
	{
		return;
	}

	if(creation_struct->id)
	{
		free(creation_struct->id);
		creation_struct->id = NULL;
	}

	if(creation_struct->created)
	{
		free(creation_struct->created);
		creation_struct->created = NULL;
	}

	if(creation_struct->created_by)
	{
		free(creation_struct->created_by);
		creation_struct->created_by = NULL;
	}


	free(creation_struct);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_creation_info_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_creation_info_t *creation_struct, bool last)
 *
 *    Serialize /Core/CreationInfo struct to JSON
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param pkgconf_buffer_t *buffer: Buffer where struct is serialized
 *    :param spdxtool_core_creation_info_t *creation_struct: CreationInfo struct to be serialized
 *    :param bool last: Is this last CreationInfo struct or does it need comma at the end. True comma False not
 *    :return: nothing
 */
void
spdxtool_core_creation_info_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_creation_info_t *creation_struct, bool last)
{
	(void) client;

	spdxtool_serialize_obj_start(buffer, 2);
	spdxtool_serialize_parm_and_string(buffer, "@type", creation_struct->type, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "@id", creation_struct->id, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "created", creation_struct->created, 3, true);
	spdxtool_serialize_parm_and_char(buffer, "createdBy", '[', 3, false);
	spdxtool_serialize_string(buffer, creation_struct->created_by, 4, false);
	spdxtool_serialize_array_end(buffer, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "specVersion", (char *)creation_struct->spec_version, 3, false);
	spdxtool_serialize_obj_end(buffer, 2, last);
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_core_creation_info_t *spdxtool_core_creation_info_new(pkgconf_client_t *client, char *agent_id, char *id)
 *
 *    Create new /Core/SpdxDocument struct
 *    In SPDX Lite SBOM there can be only one SpdxDocument
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param char *spdx_id: Id of this SpdxDocument
 *    :param char *creation_id: Id for creation info
 *    :return: NULL if some problem occurs and SpdxDocument struct if not
 */
spdxtool_core_spdx_document_t *
spdxtool_core_spdx_document_new(pkgconf_client_t *client, char *spdx_id, char *creation_id)
{
	spdxtool_core_spdx_document_t *spdx_struct = NULL;

	if(!client || !spdx_id || !creation_id)
	{
		return NULL;
	}

	(void)client;

	spdx_struct = calloc(1, sizeof(spdxtool_core_spdx_document_t));

	if(!spdx_struct)
	{
		pkgconf_error(client, "Memory exhausted! Can't create spdx_document struct.");
		return NULL;
	}

	spdx_struct->type = "SpdxDocument";
	spdx_struct->spdx_id = spdx_id;
	spdx_struct->creation_info = creation_id;
	return spdx_struct;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_spdx_document_free(spdxtool_core_spdx_document_t *spdx_struct)
 *
 *    Free /Core/SpdxDocument struct
 *
 *    :param spdxtool_core_spdx_document_t *spdx_struct: SpdxDocument struct to be freed.
 *    :return: nothing
 */
void
spdxtool_core_spdx_document_free(spdxtool_core_spdx_document_t *spdx_struct)
{
	pkgconf_node_t *iter = NULL;
	pkgconf_node_t *iter_last = NULL;

	if(!spdx_struct)
	{
		return;
	}

	if(spdx_struct->spdx_id)
	{
		free(spdx_struct->spdx_id);
		spdx_struct->spdx_id = NULL;
	}

	if(spdx_struct->creation_info)
	{
		free(spdx_struct->creation_info);
		spdx_struct->creation_info = NULL;
	}

	if(spdx_struct->agent)
	{
		free(spdx_struct->agent);
		spdx_struct->agent = NULL;
	}

	iter_last = NULL;
	PKGCONF_FOREACH_LIST_ENTRY(spdx_struct->rootElement.head, iter)
	{
		if(iter_last)
		{
			free(iter_last);
			iter_last = NULL;
		}

		spdxtool_software_sbom_t *sbom = iter->data;
		spdxtool_software_sbom_free(sbom);
		iter->data = NULL;
		iter_last = iter;
	}

	if(iter_last)
	{
		free(iter_last);
		iter_last = NULL;
	}


	iter_last = NULL;
	PKGCONF_FOREACH_LIST_ENTRY(spdx_struct->element.head, iter)
	{
		if(iter_last)
		{
			free(iter_last);
			iter_last = NULL;
		}

		free(iter->data);
		iter_last = iter;
	}

	if(iter_last)
	{
		free(iter_last);
		iter_last = NULL;
	}


	iter_last = NULL;
	PKGCONF_FOREACH_LIST_ENTRY(spdx_struct->licenses.head, iter)
	{
		if(iter_last)
		{
			free(iter_last);
			iter_last = NULL;
		}
		spdxtool_simplelicensing_license_expression_t *expression = iter->data;
		spdxtool_simplelicensing_licenseExpression_free(expression);
		iter->data = NULL;
		iter_last = iter;
	}

	if(iter_last)
	{
		free(iter_last);
		iter_last = NULL;
	}

	free(spdx_struct);
}

/*
 * !doc
 *
 * .. c:function:: bool spdxtool_core_spdx_document_is_license(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx_struct, char *license)
 *
 *    Find out if specific license is already there.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param spdxtool_core_spdx_document_t *spdx_struct: SpdxDocument struct being used.
 *    :param char *license: SPDX name of license
 *    :return: true is license is there and false if not
 */
bool
spdxtool_core_spdx_document_is_license(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx_struct, char *license)
{
	pkgconf_node_t *iter = NULL;
	spdxtool_simplelicensing_license_expression_t *expression = NULL;

	(void) client;


	if(!license)
	{
		return false;
	}

	PKGCONF_FOREACH_LIST_ENTRY(spdx_struct->licenses.head, iter)
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
 * .. c:function:: void spdxtool_core_spdx_document_add_license(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx_struct, char *license)
 *
 *    Add license to SpdxDocument and make sure that specific license is not already there.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param spdxtool_core_spdx_document_t *spdx_struct: SpdxDocument struct being used.
 *    :param char *license: SPDX name of license
 *    :return: nothing
 */
void
spdxtool_core_spdx_document_add_license(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx_struct, char *license)
{
	pkgconf_node_t *node = NULL;


	if(!license)
	{
		return;
	}

	if(spdxtool_core_spdx_document_is_license(client, spdx_struct, license))
	{
		return;
	}

	node = calloc(1, sizeof(pkgconf_node_t));
	if(!node)
	{
		pkgconf_error(client, "Memory exhausted! Cant't add license to spdx_document.");
		return;
	}
	spdxtool_simplelicensing_license_expression_t *expression = spdxtool_simplelicensing_licenseExpression_new(client, license);
	pkgconf_node_insert_tail(node, expression, &spdx_struct->licenses);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_spdx_document_add_element(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx_struct, char *element)
 *
 *    Add element spdxId to SpdxDocument
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param spdxtool_core_spdx_document_t *spdx_struct: SpdxDocument struct being used.
 *    :param char *element: spdxId of element
 *    :return: nothing
 */
void
spdxtool_core_spdx_document_add_element(pkgconf_client_t *client, spdxtool_core_spdx_document_t *spdx_struct, char *element)
{
	pkgconf_node_t *node = NULL;

	(void) client;

	if(!element)
	{
		return;
	}

	node = calloc(1, sizeof(pkgconf_node_t));
	if(!node)
	{
		pkgconf_error(NULL, "Memory exhausted! Can't add spdx_id's to spdx_document.");
		return;
	}
	pkgconf_node_insert_tail(node, element, &spdx_struct->element);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_creation_info_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_creation_info_t *creation_struct, bool last)
 *
 *    Serialize /Core/SpdxDocument struct to JSON
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param pkgconf_buffer_t *buffer: Buffer where struct is serialized
 *    :param spdxtool_core_spdx_document_t *spdx_struct: SpdxDocument struct to be serialized
 *    :param bool last: Is this last CreationInfo struct or does it need comma at the end. True comma False not
 *    :return: nothing
 */
void
spdxtool_core_spdx_document_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_spdx_document_t *spdx_struct, bool last)
{
	pkgconf_node_t *iter = NULL;
	bool is_next = false;

	PKGCONF_FOREACH_LIST_ENTRY(spdx_struct->rootElement.head, iter)
	{
		spdxtool_software_sbom_t *sbom = NULL;
		sbom = iter->data;
		spdxtool_software_sbom_serialize(client, buffer, sbom, true);
	}

	spdxtool_serialize_obj_start(buffer, 2);
	spdxtool_serialize_parm_and_string(buffer, "@type", spdx_struct->type, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "creationInfo", spdx_struct->creation_info, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "spdxId", spdx_struct->spdx_id, 3, true);
	spdxtool_serialize_parm_and_char(buffer, "rootElement", '[', 3, false);
	PKGCONF_FOREACH_LIST_ENTRY(spdx_struct->rootElement.head, iter)
	{
		spdxtool_software_sbom_t *sbom = NULL;
		sbom = iter->data;
		is_next = false;
		if(iter->next)
		{
			is_next = true;
		}
		spdxtool_serialize_string(buffer, sbom->spdx_id, 4, is_next);
	}
	spdxtool_serialize_array_end(buffer, 3, true);
	spdxtool_serialize_parm_and_char(buffer, "element", '[', 3, false);
	spdxtool_serialize_string(buffer, spdx_struct->agent, 4, true);

	PKGCONF_FOREACH_LIST_ENTRY(spdx_struct->element.head, iter)
	{
		char *spdx_id = NULL;
		spdx_id = iter->data;
		spdxtool_serialize_string(buffer, spdx_id, 4, true);
	}

	PKGCONF_FOREACH_LIST_ENTRY(spdx_struct->rootElement.head, iter)
	{
		spdxtool_software_sbom_t *sbom = NULL;
		sbom = iter->data;
		is_next = false;
		if(iter->next)
		{
			is_next = true;
		}
		spdxtool_serialize_string(buffer, sbom->spdx_id, 4, true);
		spdxtool_serialize_string(buffer, pkgconf_tuple_find(client, &sbom->rootElement->vars, "spdxId"), 4, is_next);
	}

	spdxtool_serialize_array_end(buffer, 3, false);
	spdxtool_serialize_obj_end(buffer, 2, last);
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_core_relationship_t *spdxtool_core_relationship_new(pkgconf_client_t *client, char *creation_info_id, char *spdx_id, char *from, char *to, char *relationship_type)
 *
 *    Create new /Core/Relationship struct
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param char *creation_id: Id for creation info
 *    :param char *spdx_id: Id of this SpdxDocument
 *    :param char *from: from spdxId
 *    :param char *to: to spdxId
 *    :param char *relationship_type: These can be found on SPDX documentation
 *    :return: NULL if some problem occurs and SpdxDocument struct if not
 */
spdxtool_core_relationship_t *
spdxtool_core_relationship_new(pkgconf_client_t *client, char *creation_info_id, char *spdx_id, char *from, char *to, char *relationship_type)
{
	spdxtool_core_relationship_t *relationship = NULL;

	if(!client || !creation_info_id || !spdx_id || !from || !to || !relationship_type)
	{
		return NULL;
	}

	(void) client;

	relationship = calloc(1, sizeof(spdxtool_core_relationship_t));

	if(!relationship)
	{
		pkgconf_error(client, "Memory exhausted! Can't create relationship struct.");
		return NULL;
	}

	relationship->type = "Relationship";
	relationship->creation_info = creation_info_id;
	relationship->spdx_id = spdx_id;
	relationship->from = from;
	relationship->to = to;
	relationship->relationship_type = relationship_type;

	return relationship;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_relationship_free(spdxtool_core_relationship_t *relationship_struct)
 *
 *    Free /Core/Relationship struct
 *
 *    :param spdxtool_core_relationship_t *relationship_struct: Relationship struct to be freed.
 *    :return: nothing
 */
void
spdxtool_core_relationship_free(spdxtool_core_relationship_t *relationship_struct)
{
	if(!relationship_struct)
	{
		return;
	}

	if(relationship_struct->spdx_id)
	{
		free(relationship_struct->spdx_id);
		relationship_struct->spdx_id = NULL;
	}

	if(relationship_struct->creation_info)
	{
		free(relationship_struct->creation_info);
		relationship_struct->creation_info = NULL;
	}

	if(relationship_struct->from)
	{
		free(relationship_struct->from);
		relationship_struct->from = NULL;
	}

	if(relationship_struct->to)
	{
		free(relationship_struct->to);
		relationship_struct->to = NULL;
	}

	if(relationship_struct->relationship_type)
	{
		free(relationship_struct->relationship_type);
		relationship_struct->relationship_type = NULL;
	}

	free(relationship_struct);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_core_creation_info_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_creation_info_t *creation_struct, bool last)
 *
 *    Serialize /Core/Relationship struct to JSON
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param pkgconf_buffer_t *buffer: Buffer where struct is serialized
 *    :param spdxtool_core_relationship_t *relationship_struct: Relationship struct to be serialized
 *    :param bool last: Is this last CreationInfo struct or does it need comma at the end. True comma False not
 *    :return: nothing
 */
void
spdxtool_core_relationship_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_relationship_t *relationship_struct, bool last)
{
	(void) client;

	spdxtool_serialize_obj_start(buffer, 2);
	spdxtool_serialize_parm_and_string(buffer, "@type", relationship_struct->type, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "creationInfo", relationship_struct->creation_info, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "spdxId", relationship_struct->spdx_id, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "from", relationship_struct->from, 3, true);
	spdxtool_serialize_parm_and_char(buffer, "to", '[', 3, false);
	spdxtool_serialize_string(buffer, relationship_struct->to, 4, false);
	spdxtool_serialize_array_end(buffer, 3, true);
	spdxtool_serialize_parm_and_string(buffer, "relationshipType", relationship_struct->relationship_type, 3, false);
	spdxtool_serialize_obj_end(buffer, 2, last);
}
