/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdlib.h>

#include <dom/core/element.h>
#include <dom/core/node.h>
#include <dom/core/string.h>

#include "core/document.h"
#include "core/element.h"
#include "core/namednodemap.h"
#include "core/node.h"

#include "utils/utils.h"

/**
 * DOM named node map
 */
struct dom_namednodemap {
	dom_document *owner;	/**< Owning document */

	void *priv;			/**< Private data */

	struct nnm_operation *opt;	/**< The underlaid operation 
		 			 * implementations */

	uint32_t refcnt;		/**< Reference count */
};

/**
 * Create a namednodemap
 *
 * \param doc   The owning document
 * \param priv  The private data of this dom_namednodemap
 * \param opt   The operation function pointer
 * \param map   Pointer to location to receive created map
 * \return DOM_NO_ERR on success, DOM_NO_MEM_ERR on memory exhaustion
 *
 * ::head must be a node owned by ::doc and must be either an Element or
 * DocumentType node.
 *
 * If ::head is of type Element, ::type must be DOM_ATTRIBUTE_NODE
 * If ::head is of type DocumentType, ::type may be either
 * DOM_ENTITY_NODE or DOM_NOTATION_NODE.
 *
 * The returned map will already be referenced, so the client need not
 * explicitly reference it. The client must unref the map once it is
 * finished with it.
 */
dom_exception _dom_namednodemap_create(dom_document *doc,
		void *priv, struct nnm_operation *opt,
		dom_namednodemap **map)
{
	dom_namednodemap *m;

	m = malloc(sizeof(dom_namednodemap));
	if (m == NULL)
		return DOM_NO_MEM_ERR;

	m->owner = doc;

	m->priv = priv;
	m->opt = opt;

	m->refcnt = 1;

	*map = m;

	return DOM_NO_ERR;
}

/**
 * Claim a reference on a DOM named node map
 *
 * \param map  The map to claim a reference on
 */
void dom_namednodemap_ref(dom_namednodemap *map)
{
	assert(map != NULL);
	map->refcnt++;
}

/**
 * Release a reference on a DOM named node map
 *
 * \param map  The map to release the reference from
 *
 * If the reference count reaches zero, any memory claimed by the
 * map will be released
 */
void dom_namednodemap_unref(dom_namednodemap *map)
{
	if (map == NULL)
		return;

	if (--map->refcnt == 0) {
		/* Call the implementation specific destroy */
		map->opt->namednodemap_destroy(map->priv);

		/* Destroy the map object */
		free(map);
	}
}

/**
 * Retrieve the length of a named node map
 *
 * \param map     Map to retrieve length of
 * \param length  Pointer to location to receive length
 * \return DOM_NO_ERR.
 */
dom_exception dom_namednodemap_get_length(dom_namednodemap *map,
		uint32_t *length)
{
	assert(map->opt != NULL);
	return map->opt->namednodemap_get_length(map->priv, length);
}

/**
 * Retrieve an item by name from a named node map
 *
 * \param map   The map to retrieve the item from
 * \param name  The name of the item to retrieve
 * \param node  Pointer to location to receive item
 * \return DOM_NO_ERR.
 *
 * The returned node will have had its reference count increased. The client
 * should unref the node once it has finished with it.
 */
dom_exception _dom_namednodemap_get_named_item(dom_namednodemap *map,
		dom_string *name, dom_node **node)
{
	assert(map->opt != NULL);
	return map->opt->namednodemap_get_named_item(map->priv, name, node);
}

/**
 * Add a node to a named node map, replacing any matching existing node
 *
 * \param map   The map to add to
 * \param arg   The node to add
 * \param node  Pointer to location to receive replaced node
 * \return DOM_NO_ERR                      on success,
 *         DOM_WRONG_DOCUMENT_ERR          if ::arg was created from a
 *                                         different document than ::map,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::map is readonly,
 *         DOM_INUSE_ATTRIBUTE_ERR         if ::arg is an Attr that is
 *                                         already an attribute on another
 *                                         Element,
 *         DOM_HIERARCHY_REQUEST_ERR       if the type of ::arg is not
 *                                         permitted as a member of ::map.
 *
 * ::arg's nodeName attribute will be used to store it in ::map. It will
 * be accessible using the nodeName attribute as the key for lookup.
 *
 * Replacing a node by itself has no effect.
 *
 * The returned node will have had its reference count increased. The client
 * should unref the node once it has finished with it.
 */
dom_exception _dom_namednodemap_set_named_item(dom_namednodemap *map,
		dom_node *arg, dom_node **node)
{
	assert(map->opt != NULL);
	return map->opt->namednodemap_set_named_item(map->priv, arg, node);
}

/**
 * Remove an item by name from a named node map
 *
 * \param map   The map to remove from
 * \param name  The name of the item to remove
 * \param node  Pointer to location to receive removed item
 * \return DOM_NO_ERR                      on success,
 *         DOM_NOT_FOUND_ERR               if there is no node named ::name
 *                                         in ::map,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::map is readonly.
 *
 * The returned node will have had its reference count increased. The client
 * should unref the node once it has finished with it.
 */
dom_exception _dom_namednodemap_remove_named_item(
		dom_namednodemap *map, dom_string *name,
		dom_node **node)
{
	assert(map->opt != NULL);
	return map->opt->namednodemap_remove_named_item(map->priv, name, node);
}

/**
 * Retrieve an item from a named node map
 *
 * \param map    The map to retrieve the item from
 * \param index  The map index to retrieve
 * \param node   Pointer to location to receive item
 * \return DOM_NO_ERR.
 *
 * ::index is a zero-based index into ::map.
 * ::index lies in the range [0, length-1]
 *
 * The returned node will have had its reference count increased. The client
 * should unref the node once it has finished with it.
 */
dom_exception _dom_namednodemap_item(dom_namednodemap *map,
		uint32_t index, dom_node **node)
{
	assert(map->opt != NULL);
	return map->opt->namednodemap_item(map->priv, index, node);
}

/**
 * Retrieve an item by namespace/localname from a named node map
 *
 * \param map        The map to retrieve the item from
 * \param namespace  The namespace URI of the item to retrieve
 * \param localname  The local name of the node to retrieve
 * \param node       Pointer to location to receive item
 * \return DOM_NO_ERR            on success,
 *         DOM_NOT_SUPPORTED_ERR if the implementation does not support the
 *                               feature "XML" and the language exposed
 *                               through the Document does not support
 *                               Namespaces.
 *
 * The returned node will have had its reference count increased. The client
 * should unref the node once it has finished with it.
 */
dom_exception _dom_namednodemap_get_named_item_ns(
		dom_namednodemap *map, dom_string *namespace,
		dom_string *localname, dom_node **node)
{
	assert(map->opt != NULL);
	return map->opt->namednodemap_get_named_item_ns(map->priv, namespace,
			localname, node);
}

/**
 * Add a node to a named node map, replacing any matching existing node
 *
 * \param map   The map to add to
 * \param arg   The node to add
 * \param node  Pointer to location to receive replaced node
 * \return DOM_NO_ERR                      on success,
 *         DOM_WRONG_DOCUMENT_ERR          if ::arg was created from a
 *                                         different document than ::map,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::map is readonly,
 *         DOM_INUSE_ATTRIBUTE_ERR         if ::arg is an Attr that is
 *                                         already an attribute on another
 *                                         Element,
 *         DOM_HIERARCHY_REQUEST_ERR       if the type of ::arg is not
 *                                         permitted as a member of ::map.
 *         DOM_NOT_SUPPORTED_ERR if the implementation does not support the
 *                               feature "XML" and the language exposed
 *                               through the Document does not support
 *                               Namespaces.
 *
 * ::arg's namespaceURI and localName attributes will be used to store it in
 * ::map. It will be accessible using the namespaceURI and localName
 * attributes as the keys for lookup.
 *
 * Replacing a node by itself has no effect.
 *
 * The returned node will have had its reference count increased. The client
 * should unref the node once it has finished with it.
 */
dom_exception _dom_namednodemap_set_named_item_ns(
		dom_namednodemap *map, dom_node *arg,
		dom_node **node)
{
	assert(map->opt != NULL);
	return map->opt->namednodemap_set_named_item_ns(map->priv, arg, node);
}

/**
 * Remove an item by namespace/localname from a named node map
 *
 * \param map        The map to remove from
 * \param namespace  The namespace URI of the item to remove
 * \param localname  The local name of the item to remove
 * \param node       Pointer to location to receive removed item
 * \return DOM_NO_ERR                      on success,
 *         DOM_NOT_FOUND_ERR               if there is no node named ::name
 *                                         in ::map,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::map is readonly.
 *         DOM_NOT_SUPPORTED_ERR if the implementation does not support the
 *                               feature "XML" and the language exposed
 *                               through the Document does not support
 *                               Namespaces.
 *
 * The returned node will have had its reference count increased. The client
 * should unref the node once it has finished with it.
 */
dom_exception _dom_namednodemap_remove_named_item_ns(
		dom_namednodemap *map, dom_string *namespace,
		dom_string *localname, dom_node **node)
{
	assert(map->opt != NULL);
	return map->opt->namednodemap_remove_named_item_ns(map->priv, namespace,
			localname, node);
}

/**
 * Compare whether two NamedNodeMap are equal.
 *
 */
bool _dom_namednodemap_equal(dom_namednodemap *m1, 
		dom_namednodemap *m2)
{
	assert(m1->opt != NULL);
	return (m1->opt == m2->opt && m1->opt->namednodemap_equal(m1->priv,
			m2->priv));
}

/**
 * Update the dom_namednodemap to make it as a proxy of another object
 *
 * \param map	The dom_namednodemap
 * \param priv	The private data to change to
 */
void _dom_namednodemap_update(dom_namednodemap *map, void *priv)
{
	map->priv = priv;
}
