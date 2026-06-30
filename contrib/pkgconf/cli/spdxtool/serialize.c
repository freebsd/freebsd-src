/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 *​
 *​ Copyright (C) 2026 Elizabeth Ashford.
 */

#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "core.h"
#include "software.h"
#include "simplelicensing.h"
#include "serialize.h"

static void
serialize_escape_string(pkgconf_buffer_t *buffer, const char *s)
{
	for (const char *p = s; *p; p++)
	{
		switch (*p)
		{
		case '\"':
			pkgconf_buffer_append(buffer, "\\\"");
			break;
		case '\\':
			pkgconf_buffer_append(buffer, "\\\\");
			break;
		case '\b':
			pkgconf_buffer_append(buffer, "\\b");
			break;
		case '\f':
			pkgconf_buffer_append(buffer, "\\f");
			break;
		case '\n':
			pkgconf_buffer_append(buffer, "\\n");
			break;
		case '\r':
			pkgconf_buffer_append(buffer, "\\r");
			break;
		case '\t':
			pkgconf_buffer_append(buffer, "\\t");
			break;
		default:
			if (*p < 0x20)
				pkgconf_buffer_append_fmt(buffer, "\\u%04x", (unsigned int)*p);
			else
				pkgconf_buffer_push_byte(buffer, *p);
		}
	}
}

static inline void
serialize_add_indent(pkgconf_buffer_t *buffer, unsigned int level)
{
	for (; level; level--)
		pkgconf_buffer_append(buffer, "    ");
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_value_to_buf(pkgconf_buffer_t *buffer, spdxtool_serialize_value_t *value, unsigned int indent)
 *
 *    Serialize the given JSON to the buffer
 *
 *    :param pkgconf_buffer_t *buffer: Buffer to add to.
 *    :param spdxtool_serialize_value *value: Value to serialize.
 *    :param unsigned int indent: Indent level
 *    :return: true on success, false on failure
 */
bool
spdxtool_serialize_value_to_buf(pkgconf_buffer_t *buffer, spdxtool_serialize_value_t *value, unsigned int indent)
{
	if (!buffer || !value)
		return false;

	switch(value->type) {
		case SPDXTOOL_SERIALIZE_TYPE_STRING:
			pkgconf_buffer_push_byte(buffer, '"');
			serialize_escape_string(buffer, value->value.s ? value->value.s : "");
			pkgconf_buffer_push_byte(buffer, '"');
			break;
		case SPDXTOOL_SERIALIZE_TYPE_INT:
			pkgconf_buffer_append_fmt(buffer, "%d", value->value.i);
			break;
		case SPDXTOOL_SERIALIZE_TYPE_BOOL:
			pkgconf_buffer_append(buffer, value->value.b ? "true" : "false");
			break;
		case SPDXTOOL_SERIALIZE_TYPE_NULL:
			pkgconf_buffer_append(buffer, "null");
			break;
		case SPDXTOOL_SERIALIZE_TYPE_OBJECT:
		{
			pkgconf_node_t *iter;
			pkgconf_buffer_push_byte(buffer, '{');
			pkgconf_buffer_push_byte(buffer, '\n');

			PKGCONF_FOREACH_LIST_ENTRY(value->value.o->entries.head, iter)
			{
				spdxtool_serialize_object_t *entry = iter->data;
				serialize_add_indent(buffer, indent + 1);
				pkgconf_buffer_append_fmt(buffer, "\"%s\": ", entry->key);
				spdxtool_serialize_value_to_buf(buffer, entry->value, indent + 1);
				if (iter->next)
					pkgconf_buffer_push_byte(buffer, ',');
				pkgconf_buffer_push_byte(buffer, '\n');
			}

			serialize_add_indent(buffer, indent);
			pkgconf_buffer_push_byte(buffer, '}');
			break;
		}
		case SPDXTOOL_SERIALIZE_TYPE_ARRAY:
		{
			pkgconf_node_t *iter;
			pkgconf_buffer_push_byte(buffer, '[');
			pkgconf_buffer_push_byte(buffer, '\n');

			PKGCONF_FOREACH_LIST_ENTRY(value->value.a->items.head, iter)
			{
				spdxtool_serialize_value_t *entry = iter->data;
				serialize_add_indent(buffer, indent + 1);
				spdxtool_serialize_value_to_buf(buffer, entry, indent + 1);
				if (iter->next)
					pkgconf_buffer_push_byte(buffer, ',');
				pkgconf_buffer_push_byte(buffer, '\n');
			}
			serialize_add_indent(buffer, indent);
			pkgconf_buffer_push_byte(buffer, ']');
			break;
		}
	}

	return true;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_object_add_take(spdxtool_serialize_object_list_t *object_list, const char *key, spdxtool_serialize_value_t *value)
 *
 *    Add a key-value pair to a JSON object list. The key is copied internally.
 *    The object list takes ownership of the value.
 *
 *    :param spdxtool_serialize_object_list_t *object_list: Object list to add to.
 *    :param const char *key: Key string, copied internally.
 *    :param spdxtool_serialize_value_t *value: Value to associate with the key. Ownership transfers to the object list.
 *    :return: The value added, not owned by the caller.
 */
spdxtool_serialize_value_t *
spdxtool_serialize_object_add_take(spdxtool_serialize_object_list_t *object_list, const char *key, spdxtool_serialize_value_t *value)
{
	if (!object_list || !value)
	{
		spdxtool_serialize_value_free(value);
		return NULL;
	}

	pkgconf_node_t *node = calloc(1, sizeof(pkgconf_node_t));
	spdxtool_serialize_object_t *object = calloc(1, sizeof(spdxtool_serialize_object_t));
	char *keycopy = key ? strdup(key) : strdup("");
	if (!node || !object || !keycopy)
	{
		free(node);
		free(keycopy);
		spdxtool_serialize_object_free(object);
		spdxtool_serialize_value_free(value);
		return NULL;
	}

	object->key = keycopy;
	object->value = value;
	pkgconf_node_insert_tail(node, object, &object_list->entries);
	return value;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_array_add_take(spdxtool_serialize_array_t *array, spdxtool_serialize_value_t value)
 *
 *    Add a value to a JSON array. The array takes ownership of the value.
 *
 *    :param spdxtool_serialize_array_t *array: Array to add to.
 *    :param spdxtool_serialize_value_t value: Value to append. Ownership transfers to the array.
 *    :return: The value added, not owned by the caller.
 */
spdxtool_serialize_value_t *
spdxtool_serialize_array_add_take(spdxtool_serialize_array_t *array, spdxtool_serialize_value_t *value)
{
	if (!array)
	{
		// Taking value, so free
		spdxtool_serialize_value_free(value);
		return NULL;
	}

	pkgconf_node_t *node = calloc(1, sizeof(pkgconf_node_t));
	if (!node)
	{
		spdxtool_serialize_value_free(value);
		return NULL;
	}

	pkgconf_node_insert_tail(node, value, &array->items);
	return value;
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_object_list_t *spdxtool_serialize_object_list_new(void)
 *
 *    Allocate and initialize a new empty JSON object list.
 *
 *    :return: Pointer to a new spdxtool_serialize_object_list_t, or NULL on allocation failure.
 */
spdxtool_serialize_object_list_t *
spdxtool_serialize_object_list_new(void)
{
	return calloc(1, sizeof(spdxtool_serialize_object_list_t));
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_array_t *spdxtool_serialize_array_new(void)
 *
 *    Allocate and initialize a new empty JSON array.
 *
 *    :return: Pointer to a new spdxtool_serialize_array_t, or NULL on allocation failure.
 */
spdxtool_serialize_array_t *
spdxtool_serialize_array_new(void)
{
	return calloc(1, sizeof(spdxtool_serialize_array_t));
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_value_free(spdxtool_serialize_value_t *value)
 *
 *    Free all resources owned by a JSON value. For strings, frees the string.
 *    For objects and arrays, recursively frees all children. The value pointer
 *    itself is not freed as it is assumed to be stack-allocated.
 *
 *    :param spdxtool_serialize_value_t *value: Value to free. May be NULL.
 *    :return: nothing
 */
void
spdxtool_serialize_value_free(spdxtool_serialize_value_t *value)
{
	if (!value)
		return;

	switch (value->type)
	{
		case SPDXTOOL_SERIALIZE_TYPE_STRING:
			free(value->value.s);
			break;
		case SPDXTOOL_SERIALIZE_TYPE_ARRAY:
			spdxtool_serialize_array_free(value->value.a);
			break;
		case SPDXTOOL_SERIALIZE_TYPE_OBJECT:
			spdxtool_serialize_object_list_free(value->value.o);
			break;
		default:
			// Nothing to do
			break;
	}

	free(value);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_object_free(spdxtool_serialize_object_t *object)
 *
 *    Free a JSON object entry, including its key string and owned value.
 *    The object pointer itself is not freed by this function.
 *
 *    :param spdxtool_serialize_object_t *object: Object entry to free. May be NULL.
 *    :return: nothing
 */
void
spdxtool_serialize_object_free(spdxtool_serialize_object_t *object)
{
	if (!object)
		return;

	free(object->key);
	spdxtool_serialize_value_free(object->value);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_object_list_free(spdxtool_serialize_object_list_t *object_list)
 *
 *    Free a JSON object list and all of its entries, including their keys and values.
 *
 *    :param spdxtool_serialize_object_list_t *object_list: Object list to free. May be NULL.
 *    :return: nothing
 */
void
spdxtool_serialize_object_list_free(spdxtool_serialize_object_list_t *object_list)
{
	if (!object_list)
		return;

	pkgconf_node_t *iter_next = NULL, *iter = NULL;
	PKGCONF_FOREACH_LIST_ENTRY_SAFE(object_list->entries.head, iter_next, iter)
	{
		spdxtool_serialize_object_t *object = iter->data;
		spdxtool_serialize_object_free(object);
		free(object);
		free(iter);
	}

	free(object_list);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_array_free(spdxtool_serialize_array_t *array)
 *
 *    Free a JSON array and all of its elements.
 *
 *    :param spdxtool_serialize_array_t *array: Array to free. May be NULL.
 *    :return: nothing
 */
void
spdxtool_serialize_array_free(spdxtool_serialize_array_t *array)
{
	if (!array)
		return;

	pkgconf_node_t *iter_next = NULL, *iter = NULL;
	PKGCONF_FOREACH_LIST_ENTRY_SAFE(array->items.head, iter_next, iter)
	{
		spdxtool_serialize_value_t *value = iter->data;
		spdxtool_serialize_value_free(value);
		free(iter);
	}

	free(array);
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_serialize_sbom(pkgconf_client_t *client, spdxtool_core_agent_t *agent, spdxtool_core_creation_info_t *creation, spdxtool_core_spdx_document_t *spdx)
 *
 *    Serialize a complete SPDX SBOM document to a JSON-LD value tree. Iterates
 *    all SBOMs, packages, relationships, and license expressions registered on
 *    the document. The SpdxDocument object is emitted last to ensure all element
 *    IDs have been populated by prior iteration. This function must be called
 *    after pkgconf_pkg_traverse has completed so that all packages and their
 *    dependencies are registered on spdx.
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param spdxtool_core_agent_t *agent: Agent struct to include in the document.
 *    :param spdxtool_core_creation_info_t *creation: CreationInfo struct to include in the document.
 *    :param spdxtool_core_spdx_document_t *spdx: SpdxDocument struct containing all registered SBOMs, packages, relationships, and licenses.
 *    :return: spdxtool_serialize_value_t * representing the complete JSON-LD document, or a null string value on allocation failure.
 */
spdxtool_serialize_value_t *
spdxtool_serialize_sbom(pkgconf_client_t *client, spdxtool_core_agent_t *agent, spdxtool_core_creation_info_t *creation, spdxtool_core_spdx_document_t *spdx)
{
	const char *errstr = "out of memory";
	spdxtool_serialize_value_t *ret = NULL;
	spdxtool_serialize_array_t *graph = NULL;
	spdxtool_serialize_object_list_t *root = spdxtool_serialize_object_list_new();
	if (!root)
		goto err;

	if (!spdxtool_serialize_object_add_string(root, "@context", "https://spdx.org/rdf/3.0.1/spdx-context.jsonld"))
		goto err;

	graph = spdxtool_serialize_array_new();
	if (!graph)
		goto err;

	if (!spdxtool_serialize_array_add_take(graph, spdxtool_core_agent_to_object(client, agent)))
		goto err;

	if (!spdxtool_serialize_array_add_take(graph, spdxtool_core_creation_info_to_object(client, creation)))
		goto err;

	pkgconf_node_t *iter = NULL;
	PKGCONF_FOREACH_LIST_ENTRY(spdx->licenses.head, iter)
	{
		spdxtool_simplelicensing_license_expression_t *expression = iter->data;
		if (!expression)
		{
			errstr = "licenses list corrupted";
			goto err;
		}
		if (!spdxtool_serialize_array_add_take(graph, spdxtool_simplelicensing_licenseExpression_to_object(client, spdx->creation_info, expression)))
			goto err;
	}

	PKGCONF_FOREACH_LIST_ENTRY(spdx->rootElement.head, iter)
	{
		spdxtool_software_sbom_t *current_sbom = iter->data;
		if (!current_sbom)
		{
			errstr = "sbom list corrupted";
			goto err;
		}
		if (!spdxtool_serialize_array_add_take(graph, spdxtool_software_sbom_to_object(client, current_sbom)))
			goto err;
	}

	PKGCONF_FOREACH_LIST_ENTRY(spdx->packages.head, iter)
	{
		pkgconf_pkg_t *pkg = iter->data;
		if (!pkg)
		{
			errstr = "pkg list corrupted";
			goto err;
		}
		if (!spdxtool_serialize_array_add_take(graph, spdxtool_software_package_to_object(client, pkg, spdx)))
			goto err;
	}

	PKGCONF_FOREACH_LIST_ENTRY(spdx->relationships.head, iter)
	{
		spdxtool_core_relationship_t *relationship = iter->data;
		if (!relationship)
		{
			errstr = "relationship list corrupted";
			goto err;
		}
		if (!spdxtool_serialize_array_add_take(graph, spdxtool_core_relationship_to_object(client, relationship)))
			goto err;
	}

	// SpdxDocument last — spdx->element must be fully populated first
	if (!spdxtool_serialize_array_add_take(graph, spdxtool_core_spdx_document_to_object(client, spdx)))
		goto err;

	bool ok = spdxtool_serialize_object_add_array(root, "@graph", graph);
	graph = NULL;
	if (!ok)
		goto err;

	ret = spdxtool_serialize_value_object(root);
	root = NULL;

err:
	if (!ret)
		pkgconf_error(client, "spdxtool_serialize_sbom: %s", errstr);

	spdxtool_serialize_object_list_free(root);
	spdxtool_serialize_array_free(graph);
	return ret;
}
