/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dom/core/attr.h>
#include <dom/core/text.h>
#include <dom/core/document.h>
#include <dom/core/namednodemap.h>
#include <dom/core/nodelist.h>
#include <dom/core/implementation.h>
#include <dom/core/document_type.h>
#include <dom/events/events.h>

#include "core/string.h"
#include "core/namednodemap.h"
#include "core/attr.h"
#include "core/cdatasection.h"
#include "core/comment.h"
#include "core/document.h"
#include "core/document_type.h"
#include "core/doc_fragment.h"
#include "core/element.h"
#include "core/entity_ref.h"
#include "core/node.h"
#include "core/pi.h"
#include "core/text.h"
#include "utils/utils.h"
#include "utils/validate.h"
#include "events/mutation_event.h"

static bool _dom_node_permitted_child(const dom_node_internal *parent, 
		const dom_node_internal *child);
static inline dom_exception _dom_node_attach(dom_node_internal *node, 
		dom_node_internal *parent,
		dom_node_internal *previous, 
		dom_node_internal *next);
static inline void _dom_node_detach(dom_node_internal *node);
static inline dom_exception _dom_node_attach_range(dom_node_internal *first, 
		dom_node_internal *last,
		dom_node_internal *parent, 
		dom_node_internal *previous, 
		dom_node_internal *next);
static inline dom_exception _dom_node_detach_range(dom_node_internal *first,
		dom_node_internal *last);
static inline void _dom_node_replace(dom_node_internal *old, 
		dom_node_internal *replacement);

static struct dom_node_vtable node_vtable = {
	{
		DOM_NODE_EVENT_TARGET_VTABLE
	},
	DOM_NODE_VTABLE
};

static struct dom_node_protect_vtable node_protect_vtable = {
	DOM_NODE_PROTECT_VTABLE
};



/*----------------------------------------------------------------------*/

/* The constructor and destructor of this object */

/* Create a DOM node and compose the vtable */
dom_node_internal * _dom_node_create(void)
{
	dom_node_internal *node = malloc(sizeof(struct dom_node_internal));
	if (node == NULL)
		return NULL;

	node->base.vtable = &node_vtable;
	node->vtable = &node_protect_vtable;
	return node;
}

/**
 * Destroy a DOM node
 *
 * \param node  The node to destroy
 *
 * ::node's parent link must be NULL and its reference count must be 0.
 *
 * ::node will be freed.
 *
 * This function should only be called from dom_node_unref or type-specific
 * destructors (for destroying child nodes). Anything else should not
 * be attempting to destroy nodes -- they should simply be unreferencing
 * them (so destruction will occur at the appropriate time).
 */
void _dom_node_destroy(struct dom_node_internal *node)
{
	struct dom_document *owner = node->owner;
	bool null_owner_permitted = (node->type == DOM_DOCUMENT_NODE || 
			node->type == DOM_DOCUMENT_TYPE_NODE);

	assert(null_owner_permitted || owner != NULL); 

	if (!null_owner_permitted) {
		/* Claim a reference upon the owning document during 
		 * destruction to ensure that the document doesn't get 
		 * destroyed before its contents. */
		dom_node_ref(owner);
	}

	/* Finalise this node, this should also destroy all the child nodes. */
	_dom_node_finalise(node);

	if (!null_owner_permitted) {
		/* Release the reference we claimed on the document. If this
		 * is the last reference held on the document and the list
		 * of nodes pending deletion is empty, then the document will
		 * be destroyed. */
		dom_node_unref(owner);
	}

	/* Release our memory */
	free(node);
}

/**
 * Initialise a DOM node
 *
 * \param node       The node to initialise
 * \param doc        The document which owns the node
 * \param type       The node type required
 * \param name       The node (local) name, or NULL
 * \param value      The node value, or NULL
 * \param namespace  Namespace URI to use for node, or NULL
 * \param prefix     Namespace prefix to use for node, or NULL
 * \return DOM_NO_ERR on success.
 *
 * ::name, ::value, ::namespace, and ::prefix  will have their reference 
 * counts increased.
 */
dom_exception _dom_node_initialise(dom_node_internal *node,
		struct dom_document *doc, dom_node_type type,
		dom_string *name, dom_string *value,
		dom_string *namespace, dom_string *prefix)
{
	node->owner = doc;

	if (name != NULL)
		node->name = dom_string_ref(name);
	else
		node->name = NULL;

	if (value != NULL)
		node->value = dom_string_ref(value);
	else
		node->value = NULL;

	node->type = type;

	node->parent = NULL;
	node->first_child = NULL;
	node->last_child = NULL;
	node->previous = NULL;
	node->next = NULL;

	/* Note: nodes do not reference the document to which they belong,
	 * as this would result in the document never being destroyed once
	 * the client has finished with it. The document will be aware of
	 * any nodes that it owns through 2 mechanisms:
	 *
	 * either a) Membership of the document tree
	 * or     b) Membership of the list of nodes pending deletion
	 *
	 * It is not possible for any given node to be a member of both
	 * data structures at the same time.
	 *
	 * The document will not be destroyed until both of these
	 * structures are empty. It will forcibly attempt to empty
	 * the document tree on document destruction. Any still-referenced
	 * nodes at that time will be added to the list of nodes pending
	 * deletion. This list will not be forcibly emptied, as it contains
	 * those nodes (and their sub-trees) in use by client code.
	 */

	if (namespace != NULL)
		node->namespace = dom_string_ref(namespace);
	else
		node->namespace = NULL;

	if (prefix != NULL)
		node->prefix = dom_string_ref(prefix);
	else
		node->prefix = NULL;

	node->user_data = NULL;

	node->base.refcnt = 1;

	list_init(&node->pending_list);
	if (node->type != DOM_DOCUMENT_NODE) {
		/* A Node should be in the pending list when it is created */
		dom_node_mark_pending(node);
	}

	return _dom_event_target_internal_initialise(&node->eti);
}

/**
 * Finalise a DOM node
 *
 * \param node  The node to finalise
 *
 * The contents of ::node will be cleaned up. ::node will not be freed.
 * All children of ::node should have been removed prior to finalisation.
 */
void _dom_node_finalise(dom_node_internal *node)
{
	struct dom_user_data *u, *v;
	struct dom_node_internal *p;
	struct dom_node_internal *n = NULL;

	/* Destroy user data */
	for (u = node->user_data; u != NULL; u = v) {
		v = u->next;

		if (u->handler != NULL)
			u->handler(DOM_NODE_DELETED, u->key, u->data, 
					NULL, NULL);

		dom_string_unref(u->key);
		free(u);
	}
	node->user_data = NULL;

	if (node->prefix != NULL) {
		dom_string_unref(node->prefix);
		node->prefix = NULL;
	}

	if (node->namespace != NULL) {
		dom_string_unref(node->namespace);
		node->namespace = NULL;
	}

	/* Destroy all the child nodes of this node */
	p = node->first_child;
	while (p != NULL) {
		n = p->next;
		p->parent = NULL;
		dom_node_try_destroy(p);
		p = n;
	}

	/* Paranoia */
	node->next = NULL;
	node->previous = NULL;
	node->last_child = NULL;
	node->first_child = NULL;
	node->parent = NULL;

	if (node->value != NULL) {
		dom_string_unref(node->value);
		node->value = NULL;
	}

	if (node->name != NULL) {
		dom_string_unref(node->name);
		node->name = NULL;
	}

	/* If the node has no owner document, we need not to finalise its
	 * dom_event_target_internal structure. 
	 */
	if (node->owner != NULL)
		_dom_event_target_internal_finalise(&node->eti);

	/* Detach from the pending list, if we are in it,
	 * this part of code should always be the end of this function. */
	if (node->pending_list.prev != &node->pending_list) {
		assert (node->pending_list.next != &node->pending_list); 
		list_del(&node->pending_list);
		if (node->owner != NULL && node->type != DOM_DOCUMENT_NODE) {
			/* Deleting this node from the pending list may cause
			 * the list to be null and we should try to destroy 
			 * the document. */
			_dom_document_try_destroy(node->owner);
		}
	}
}


/* ---------------------------------------------------------------------*/

/* The public virtual function of this interface Node */

/**
 * Retrieve the name of a DOM node
 *
 * \param node    The node to retrieve the name of
 * \param result  Pointer to location to receive node name
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception _dom_node_get_node_name(dom_node_internal *node,
		dom_string **result)
{
	dom_string *node_name, *temp;
	dom_exception err;

	/* Document Node and DocumentType Node can have no owner */
	assert(node->type == DOM_DOCUMENT_TYPE_NODE ||
			node->type == DOM_DOCUMENT_NODE ||
			node->owner != NULL);

	assert(node->name != NULL);

	/* If this node was created using a namespace-aware method and
	 * has a defined prefix, then nodeName is a QName comprised
	 * of prefix:name. */
	if (node->prefix != NULL) {
		dom_string *colon;

		err = dom_string_create((const uint8_t *) ":", SLEN(":"), 
				&colon);
		if (err != DOM_NO_ERR) {
			return err;
		}

		/* Prefix + : */
		err = dom_string_concat(node->prefix, colon, &temp);
		if (err != DOM_NO_ERR) {
			dom_string_unref(colon);
			return err;
		}

		/* Finished with colon */
		dom_string_unref(colon);

		/* Prefix + : + Localname */
		err = dom_string_concat(temp, node->name, &node_name);
		if (err != DOM_NO_ERR) {
			dom_string_unref(temp);
			return err;
		}

		/* Finished with temp */
		dom_string_unref(temp);
	} else {
		node_name = dom_string_ref(node->name);
	}

	*result = node_name;

	return DOM_NO_ERR;
}

/**
 * Retrieve the value of a DOM node
 *
 * \param node    The node to retrieve the value of
 * \param result  Pointer to location to receive node value
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 *
 * DOM3Core states that this can raise DOMSTRING_SIZE_ERR. It will not in
 * this implementation; dom_strings are unbounded.
 */
dom_exception _dom_node_get_node_value(dom_node_internal *node,
		dom_string **result)
{
	if (node->value != NULL)
		dom_string_ref(node->value);

	*result = node->value;

	return DOM_NO_ERR;
}

/**
 * Set the value of a DOM node
 *
 * \param node   Node to set the value of
 * \param value  New value for node
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if the node is readonly and the
 *                                         value is not defined to be null.
 *
 * The new value will have its reference count increased, so the caller
 * should unref it after the call (as the caller should have already claimed
 * a reference on the string). The node's existing value will be unrefed.
 */
dom_exception _dom_node_set_node_value(dom_node_internal *node,
		dom_string *value)
{
	/* TODO
	 * Whether we should change this to a virtual function? 
	 */
	/* This is a NOP if the value is defined to be null. */
	if (node->type == DOM_DOCUMENT_NODE || 
			node->type == DOM_DOCUMENT_FRAGMENT_NODE || 
			node->type == DOM_DOCUMENT_TYPE_NODE || 
			node->type == DOM_ELEMENT_NODE || 
			node->type == DOM_ENTITY_NODE || 
			node->type == DOM_ENTITY_REFERENCE_NODE || 
			node->type == DOM_NOTATION_NODE) {
		return DOM_NO_ERR;
	}

	/* Ensure node is writable */
	if (_dom_node_readonly(node))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	/* If it's an attribute node, then delegate setting to 
	 * the type-specific function */
	if (node->type == DOM_ATTRIBUTE_NODE)
		return dom_attr_set_value((struct dom_attr *) node, value);

	if (node->value != NULL)
		dom_string_unref(node->value);

	if (value != NULL)
		dom_string_ref(value);

	node->value = value;

	return DOM_NO_ERR;
}

/**
 * Retrieve the type of a DOM node
 *
 * \param node    The node to retrieve the type of
 * \param result  Pointer to location to receive node type
 * \return DOM_NO_ERR.
 */
dom_exception _dom_node_get_node_type(dom_node_internal *node, 
		dom_node_type *result)
{
	*result = node->type;

	return DOM_NO_ERR;
}

/**
 * Retrieve the parent of a DOM node
 *
 * \param node    The node to retrieve the parent of
 * \param result  Pointer to location to receive node parent
 * \return DOM_NO_ERR.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_node_get_parent_node(dom_node_internal *node,
		dom_node_internal **result)
{
	/* Attr nodes have no parent */
	if (node->type == DOM_ATTRIBUTE_NODE) {
		*result = NULL;
		return DOM_NO_ERR;
	}

	/* If there is a parent node, then increase its reference count */
	if (node->parent != NULL)
		dom_node_ref(node->parent);

	*result = node->parent;

	return DOM_NO_ERR;
}

/**
 * Retrieve a list of children of a DOM node
 *
 * \param node    The node to retrieve the children of
 * \param result  Pointer to location to receive child list
 * \return DOM_NO_ERR.
 *
 * The returned NodeList will be referenced. It is the responsibility
 * of the caller to unref the list once it has finished with it.
 */
dom_exception _dom_node_get_child_nodes(dom_node_internal *node,
		struct dom_nodelist **result)
{
	/* Can't do anything without an owning document.
	 * This is only a problem for DocumentType nodes 
	 * which are not yet attached to a document. 
	 * DocumentType nodes have no children, anyway. */
	if (node->owner == NULL)
		return DOM_NOT_SUPPORTED_ERR;

	return _dom_document_get_nodelist(node->owner, DOM_NODELIST_CHILDREN,
			node, NULL, NULL, NULL, result);
}

/**
 * Retrieve the first child of a DOM node
 *
 * \param node    The node to retrieve the first child of
 * \param result  Pointer to location to receive node's first child
 * \return DOM_NO_ERR.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_node_get_first_child(dom_node_internal *node,
		dom_node_internal **result)
{
	/* If there is a first child, increase its reference count */
	if (node->first_child != NULL)
		dom_node_ref(node->first_child);

	*result = node->first_child;

	return DOM_NO_ERR;
}

/**
 * Retrieve the last child of a DOM node
 *
 * \param node    The node to retrieve the last child of
 * \param result  Pointer to location to receive node's last child
 * \return DOM_NO_ERR.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_node_get_last_child(dom_node_internal *node,
		dom_node_internal **result)
{
	/* If there is a last child, increase its reference count */
	if (node->last_child != NULL)
		dom_node_ref(node->last_child);

	*result = node->last_child;

	return DOM_NO_ERR;
}

/**
 * Retrieve the previous sibling of a DOM node
 *
 * \param node    The node to retrieve the previous sibling of
 * \param result  Pointer to location to receive node's previous sibling
 * \return DOM_NO_ERR.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_node_get_previous_sibling(dom_node_internal *node,
		dom_node_internal **result)
{
	/* Attr nodes have no previous siblings */
	if (node->type == DOM_ATTRIBUTE_NODE) {
		*result = NULL;
		return DOM_NO_ERR;
	}

	/* If there is a previous sibling, increase its reference count */
	if (node->previous != NULL)
		dom_node_ref(node->previous);

	*result = node->previous;

	return DOM_NO_ERR;
}

/**
 * Retrieve the subsequent sibling of a DOM node
 *
 * \param node    The node to retrieve the subsequent sibling of
 * \param result  Pointer to location to receive node's subsequent sibling
 * \return DOM_NO_ERR.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_node_get_next_sibling(dom_node_internal *node,
		dom_node_internal **result)
{
	/* Attr nodes have no next siblings */
	if (node->type == DOM_ATTRIBUTE_NODE) {
		*result = NULL;
		return DOM_NO_ERR;
	}

	/* If there is a subsequent sibling, increase its reference count */
	if (node->next != NULL)
		dom_node_ref(node->next);

	*result = node->next;

	return DOM_NO_ERR;
}

/**
 * Retrieve a map of attributes associated with a DOM node
 *
 * \param node    The node to retrieve the attributes of
 * \param result  Pointer to location to receive attribute map
 * \return DOM_NO_ERR.
 *
 * The returned NamedNodeMap will be referenced. It is the responsibility
 * of the caller to unref the map once it has finished with it.
 *
 * If ::node is not an Element, then NULL will be returned.
 */
dom_exception _dom_node_get_attributes(dom_node_internal *node,
		struct dom_namednodemap **result)
{
	UNUSED(node);
	*result = NULL;

	return DOM_NO_ERR;
}

/**
 * Retrieve the owning document of a DOM node
 *
 * \param node    The node to retrieve the owner of
 * \param result  Pointer to location to receive node's owner
 * \return DOM_NO_ERR.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_node_get_owner_document(dom_node_internal *node,
		struct dom_document **result)
{
	/* Document nodes have no owner, as far as clients are concerned 
	 * In reality, they own themselves as this simplifies code elsewhere */
	if (node->type == DOM_DOCUMENT_NODE) {
		*result = NULL;

		return DOM_NO_ERR;
	}

	/* If there is an owner, increase its reference count */
	if (node->owner != NULL)
		dom_node_ref(node->owner);

	*result = node->owner;

	return DOM_NO_ERR;
}

/**
 * Insert a child into a node
 *
 * \param node       Node to insert into
 * \param new_child  Node to insert
 * \param ref_child  Node to insert before, or NULL to insert as last child
 * \param result     Pointer to location to receive node being inserted
 * \return DOM_NO_ERR                      on success,
 *         DOM_HIERARCHY_REQUEST_ERR       if ::new_child's type is not
 *                                         permitted as a child of ::node,
 *                                         or ::new_child is an ancestor of
 *                                         ::node (or is ::node itself), or
 *                                         ::node is of type Document and a
 *                                         second DocumentType or Element is
 *                                         being inserted,
 *         DOM_WRONG_DOCUMENT_ERR          if ::new_child was created from a
 *                                         different document than ::node,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::node is readonly, or
 *                                         ::new_child's parent is readonly,
 *         DOM_NOT_FOUND_ERR               if ::ref_child is not a child of
 *                                         ::node.
 *
 * If ::new_child is a DocumentFragment, all of its children are inserted.
 * If ::new_child is already in the tree, it is first removed.
 *
 * Attempting to insert a node before itself is a NOP.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_node_insert_before(dom_node_internal *node,
		dom_node_internal *new_child, dom_node_internal *ref_child,
		dom_node_internal **result)
{
	dom_exception err;
	dom_node_internal *n;
	
	assert(node != NULL);
	
	/* Ensure that new_child and node are owned by the same document */
	if ((new_child->type == DOM_DOCUMENT_TYPE_NODE && 
			new_child->owner != NULL && 
			new_child->owner != node->owner) ||
			(new_child->type != DOM_DOCUMENT_TYPE_NODE &&
			new_child->owner != node->owner))
		return DOM_WRONG_DOCUMENT_ERR;

	/* Ensure node isn't read only */
	if (_dom_node_readonly(node))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	/* Ensure that ref_child (if any) is a child of node */
	if (ref_child != NULL && ref_child->parent != node)
		return DOM_NOT_FOUND_ERR;
	
	/* Ensure that new_child is not an ancestor of node, nor node itself */
	for (n = node; n != NULL; n = n->parent) {
		if (n == new_child)
			return DOM_HIERARCHY_REQUEST_ERR;
	}

	/* Ensure that new_child is permitted as a child of node */
	if (new_child->type != DOM_DOCUMENT_FRAGMENT_NODE && 
			!_dom_node_permitted_child(node, new_child))
		return DOM_HIERARCHY_REQUEST_ERR;

	/* Attempting to insert a node before itself is a NOP */
	if (new_child == ref_child) {
		dom_node_ref(new_child);
		*result = new_child;

		return DOM_NO_ERR;
	}

	/* If new_child is already in the tree and 
	 * its parent isn't read only, remove it */
	if (new_child->parent != NULL) {
		if (_dom_node_readonly(new_child->parent))
			return DOM_NO_MODIFICATION_ALLOWED_ERR;

		_dom_node_detach(new_child);
	}

	/* When a Node is attached, it should be removed from the pending 
	 * list */
	dom_node_remove_pending(new_child);

	/* If new_child is a DocumentFragment, insert its children.
	 * Otherwise, insert new_child */
	if (new_child->type == DOM_DOCUMENT_FRAGMENT_NODE) {
		/* Test the children of the docment fragment can be appended */
		dom_node_internal *c = new_child->first_child;
		for (; c != NULL; c = c->next)
			if (!_dom_node_permitted_child(node, c))
				return DOM_HIERARCHY_REQUEST_ERR;

		if (new_child->first_child != NULL) {
			err = _dom_node_attach_range(new_child->first_child,
					new_child->last_child, 
					node, 
					ref_child == NULL ? node->last_child 
							  : ref_child->previous,
					ref_child == NULL ? NULL 
							  : ref_child);
			if (err != DOM_NO_ERR)
				return err;

			new_child->first_child = NULL;
			new_child->last_child = NULL;
		}
	} else {
		err = _dom_node_attach(new_child, 
				node, 
				ref_child == NULL ? node->last_child 
						  : ref_child->previous, 
				ref_child == NULL ? NULL 
						  : ref_child);
		if (err != DOM_NO_ERR)
			return err;

	}

	/* DocumentType nodes are created outside the Document so, 
	 * if we're trying to attach a DocumentType node, then we
	 * also need to set its owner. */
	if (node->type == DOM_DOCUMENT_NODE &&
			new_child->type == DOM_DOCUMENT_TYPE_NODE) {
		/* See long comment in _dom_node_initialise as to why 
		 * we don't ref the document here */
		new_child->owner = (struct dom_document *) node;
	}

	/** \todo Is it correct to return DocumentFragments? */

	dom_node_ref(new_child);
	*result = new_child;

	return DOM_NO_ERR;
}

/**
 * Replace a node's child with a new one
 *
 * \param node       Node whose child to replace
 * \param new_child  Replacement node
 * \param old_child  Child to replace
 * \param result     Pointer to location to receive replaced node
 * \return DOM_NO_ERR                      on success,
 *         DOM_HIERARCHY_REQUEST_ERR       if ::new_child's type is not
 *                                         permitted as a child of ::node,
 *                                         or ::new_child is an ancestor of
 *                                         ::node (or is ::node itself), or
 *                                         ::node is of type Document and a
 *                                         second DocumentType or Element is
 *                                         being inserted,
 *         DOM_WRONG_DOCUMENT_ERR          if ::new_child was created from a
 *                                         different document than ::node,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::node is readonly, or
 *                                         ::new_child's parent is readonly,
 *         DOM_NOT_FOUND_ERR               if ::old_child is not a child of
 *                                         ::node,
 *         DOM_NOT_SUPPORTED_ERR           if ::node is of type Document and
 *                                         ::new_child is of type
 *                                         DocumentType or Element.
 *
 * If ::new_child is a DocumentFragment, ::old_child is replaced by all of
 * ::new_child's children.
 * If ::new_child is already in the tree, it is first removed.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_node_replace_child(dom_node_internal *node,
		dom_node_internal *new_child, dom_node_internal *old_child,
		dom_node_internal **result)
{
	dom_node_internal *n;

	/* We don't support replacement of DocumentType or root Elements */
	if (node->type == DOM_DOCUMENT_NODE && 
			(new_child->type == DOM_DOCUMENT_TYPE_NODE || 
			new_child->type == DOM_ELEMENT_NODE))
		return DOM_NOT_SUPPORTED_ERR;

	/* Ensure that new_child and node are owned by the same document */
	if (new_child->owner != node->owner)
		return DOM_WRONG_DOCUMENT_ERR;

	/* Ensure node isn't read only */
	if (_dom_node_readonly(node))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	/* Ensure that old_child is a child of node */
	if (old_child->parent != node)
		return DOM_NOT_FOUND_ERR;

	/* Ensure that new_child is not an ancestor of node, nor node itself */
	for (n = node; n != NULL; n = n->parent) {
		if (n == new_child)
			return DOM_HIERARCHY_REQUEST_ERR;
	}

	/* Ensure that new_child is permitted as a child of node */
	if (new_child->type == DOM_DOCUMENT_FRAGMENT_NODE) {
		/* If this node is a doc fragment, we should test all its 
		 * children nodes */
		dom_node_internal *c;
		c = new_child->first_child;
		while (c != NULL) {
			if (!_dom_node_permitted_child(node, c))
				return DOM_HIERARCHY_REQUEST_ERR;

			c = c->next;
		}
	} else {
		if (!_dom_node_permitted_child(node, new_child))
			return DOM_HIERARCHY_REQUEST_ERR;
	}

	/* Attempting to replace a node with itself is a NOP */
	if (new_child == old_child) {
		dom_node_ref(old_child);
		*result = old_child;

		return DOM_NO_ERR;
	}

	/* If new_child is already in the tree and 
	 * its parent isn't read only, remove it */
	if (new_child->parent != NULL) {
		if (_dom_node_readonly(new_child->parent))
			return DOM_NO_MODIFICATION_ALLOWED_ERR;

		_dom_node_detach(new_child);
	}

	/* When a Node is attached, it should be removed from the pending 
	 * list */
	dom_node_remove_pending(new_child);

	/* Perform the replacement */
	_dom_node_replace(old_child, new_child);

	/* Sort out the return value */
	dom_node_ref(old_child);
	/* The replaced node should be marded pending */
	dom_node_mark_pending(old_child);
	*result = old_child;

	return DOM_NO_ERR;
}

/**
 * Remove a child from a node
 *
 * \param node       Node whose child to replace
 * \param old_child  Child to remove
 * \param result     Pointer to location to receive removed node
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::node is readonly
 *         DOM_NOT_FOUND_ERR               if ::old_child is not a child of
 *                                         ::node,
 *         DOM_NOT_SUPPORTED_ERR           if ::node is of type Document and
 *                                         ::new_child is of type
 *                                         DocumentType or Element.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_node_remove_child(dom_node_internal *node,
		dom_node_internal *old_child,
		dom_node_internal **result)
{
	dom_exception err;
	bool success = true;

	/* We don't support removal of DocumentType or root Element nodes */
	if (node->type == DOM_DOCUMENT_NODE &&
			(old_child->type == DOM_DOCUMENT_TYPE_NODE ||
			old_child->type == DOM_ELEMENT_NODE))
		return DOM_NOT_SUPPORTED_ERR;

	/* Ensure old_child is a child of node */
	if (old_child->parent != node)
		return DOM_NOT_FOUND_ERR;

	/* Ensure node is writable */
	if (_dom_node_readonly(node))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	/* Dispatch a DOMNodeRemoval event */
	err = dom_node_dispatch_node_change_event(node->owner, old_child, node,
			DOM_MUTATION_REMOVAL, &success);
	if (err != DOM_NO_ERR)
		return err;

	/* Detach the node */
	_dom_node_detach(old_child);

	/* When a Node is removed, it should be destroy. When its refcnt is not 
	 * zero, it will be added to the document's deletion pending list. 
	 * When a Node is removed, its parent should be NULL, but its owner
	 * should remain to be the document. */
	dom_node_ref(old_child);
	dom_node_try_destroy(old_child);
	*result = old_child;

	success = true;
	err = _dom_dispatch_subtree_modified_event(node->owner, node,
			&success);
	if (err != DOM_NO_ERR)
		return err;

	return DOM_NO_ERR;
}

/**
 * Append a child to the end of a node's child list
 *
 * \param node       Node to insert into
 * \param new_child  Node to append
 * \param result     Pointer to location to receive node being inserted
 * \return DOM_NO_ERR                      on success,
 *         DOM_HIERARCHY_REQUEST_ERR       if ::new_child's type is not
 *                                         permitted as a child of ::node,
 *                                         or ::new_child is an ancestor of
 *                                         ::node (or is ::node itself), or
 *                                         ::node is of type Document and a
 *                                         second DocumentType or Element is
 *                                         being inserted,
 *         DOM_WRONG_DOCUMENT_ERR          if ::new_child was created from a
 *                                         different document than ::node,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::node is readonly, or
 *                                         ::new_child's parent is readonly.
 *
 * If ::new_child is a DocumentFragment, all of its children are inserted.
 * If ::new_child is already in the tree, it is first removed.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_node_append_child(dom_node_internal *node,
		dom_node_internal *new_child,
		dom_node_internal **result)
{
	/* This is just a veneer over insert_before */
	return dom_node_insert_before(node, new_child, NULL, result);
}

/**
 * Determine if a node has any children
 *
 * \param node    Node to inspect
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_node_has_child_nodes(dom_node_internal *node, bool *result)
{
	*result = node->first_child != NULL;

	return DOM_NO_ERR;
}

/**
 * Clone a DOM node
 *
 * \param node    The node to clone
 * \param deep    True to deep-clone the node's sub-tree
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR        on success,
 *         DOM_NO_MEMORY_ERR on memory exhaustion.
 *
 * The returned node will already be referenced.
 *
 * The duplicate node will have no parent and no user data.
 *
 * If ::node has registered user_data_handlers, then they will be called.
 *
 * Cloning an Element copies all attributes & their values (including those
 * generated by the XML processor to represent defaulted attributes). It
 * does not copy any child nodes unless it is a deep copy (this includes
 * text contained within the Element, as the text is contained in a child
 * Text node).
 *
 * Cloning an Attr directly, as opposed to cloning as part of an Element,
 * returns a specified attribute. Cloning an Attr always clones its children,
 * since they represent its value, no matter whether this is a deep clone or
 * not.
 *
 * Cloning an EntityReference automatically constructs its subtree if a
 * corresponding Entity is available, no matter whether this is a deep clone
 * or not.
 *
 * Cloning any other type of node simply returns a copy.
 *
 * Note that cloning an immutable subtree results in a mutable copy, but
 * the children of an EntityReference clone are readonly. In addition, clones
 * of unspecified Attr nodes are specified.
 *
 * \todo work out what happens when cloning Document, DocumentType, Entity
 * and Notation nodes.
 *
 * Note: we adopt a OO paradigm, this clone_node just provide a basic operation
 * of clone. Special clones like Attr/EntitiReference stated above should 
 * provide their overload of this interface in their implementation file. 
 */
dom_exception _dom_node_clone_node(dom_node_internal *node, bool deep,
		dom_node_internal **result)
{
	dom_node_internal *n, *child, *r;
	dom_exception err;
	dom_user_data *ud;

	assert(node->owner != NULL);

	err = dom_node_copy(node, &n);
	if (err != DOM_NO_ERR) {
		return err;
	}

	if (deep) {
		child = node->first_child;
		while (child != NULL) {
			err = dom_node_clone_node(child, deep, (void *) &r);
			if (err != DOM_NO_ERR) {
				dom_node_unref(n);
				return err;
			}

			err = dom_node_append_child(n, r, (void *) &r);
			if (err != DOM_NO_ERR) {
				dom_node_unref(n);
				return err;
			}
			
			/* Clean up the new node, we have reference it two
			 * times */
			dom_node_unref(r);
			dom_node_unref(r);
			child = child->next;
		}
	}

	*result = n;

	/* Call the dom_user_data_handlers */
	ud = node->user_data;
	while (ud != NULL) {
		if (ud->handler != NULL)
			ud->handler(DOM_NODE_CLONED, ud->key, ud->data, 
					(dom_node *) node, (dom_node *) n);
		ud = ud->next;
	}

	return DOM_NO_ERR;
}

/**
 * Normalize a DOM node
 *
 * \param node  The node to normalize
 * \return DOM_NO_ERR.
 *
 * Puts all Text nodes in the full depth of the sub-tree beneath ::node,
 * including Attr nodes into "normal" form, where only structure separates
 * Text nodes.
 */
dom_exception _dom_node_normalize(dom_node_internal *node)
{
	dom_node_internal *n, *p;
	dom_exception err;

	p = node->first_child;
	if (p == NULL)
		return DOM_NO_ERR;

	n = p->next;

	while (n != NULL) {
		if (n->type == DOM_TEXT_NODE && p->type == DOM_TEXT_NODE) {
			err = _dom_merge_adjacent_text(p, n);
			if (err != DOM_NO_ERR)
				return err;

			_dom_node_detach(n);
			dom_node_unref(n);
			n = p->next;
			continue;
		}
		if (n->type != DOM_TEXT_NODE) {
			err = dom_node_normalize(n);
			if (err != DOM_NO_ERR)
				return err;
		}
		p = n;
		n = n->next;
	}

	return DOM_NO_ERR;
}

/**
 * Test whether the DOM implementation implements a specific feature and
 * that feature is supported by the node.
 *
 * \param node     The node to test
 * \param feature  The name of the feature to test
 * \param version  The version number of the feature to test
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_node_is_supported(dom_node_internal *node,
		dom_string *feature, dom_string *version,
		bool *result)
{
	bool has;

	UNUSED(node);

	dom_implementation_has_feature(dom_string_data(feature),
			dom_string_data(version), &has);

	*result = has;

	return DOM_NO_ERR;
}

/**
 * Retrieve the namespace of a DOM node
 *
 * \param node    The node to retrieve the namespace of
 * \param result  Pointer to location to receive node's namespace
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception _dom_node_get_namespace(dom_node_internal *node,
		dom_string **result)
{
	assert(node->owner != NULL);

	/* If there is a namespace, increase its reference count */
	if (node->namespace != NULL)
		*result = dom_string_ref(node->namespace);
	else
		*result = NULL;

	return DOM_NO_ERR;
}

/**
 * Retrieve the prefix of a DOM node
 *
 * \param node    The node to retrieve the prefix of
 * \param result  Pointer to location to receive node's prefix
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception _dom_node_get_prefix(dom_node_internal *node,
		dom_string **result)
{
	assert(node->owner != NULL);
	
	/* If there is a prefix, increase its reference count */
	if (node->prefix != NULL)
		*result = dom_string_ref(node->prefix);
	else
		*result = NULL;

	return DOM_NO_ERR;
}

/**
 * Set the prefix of a DOM node
 *
 * \param node    The node to set the prefix of
 * \param prefix  Pointer to prefix string
 * \return DOM_NO_ERR                      on success,
 *         DOM_INVALID_CHARACTER_ERR       if the specified prefix contains
 *                                         an illegal character,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::node is readonly,
 *         DOM_NAMESPACE_ERR               if the specified prefix is
 *                                         malformed, if the namespaceURI of
 *                                         ::node is null, if the specified
 *                                         prefix is "xml" and the
 *                                         namespaceURI is different from
 *                                         "http://www.w3.org/XML/1998/namespace",
 *                                         if ::node is an attribute and the
 *                                         specified prefix is "xmlns" and
 *                                         the namespaceURI is different from
 *                                         "http://www.w3.org/2000/xmlns",
 *                                         or if this node is an attribute
 *                                         and the qualifiedName of ::node
 *                                         is "xmlns".
 */
dom_exception _dom_node_set_prefix(dom_node_internal *node,
		dom_string *prefix)
{
	/* Only Element and Attribute nodes created using 
	 * namespace-aware methods may have a prefix */
	if ((node->type != DOM_ELEMENT_NODE &&
			node->type != DOM_ATTRIBUTE_NODE) || 
			node->namespace == NULL) {
		return DOM_NO_ERR;
	}

	/** \todo validate prefix */

	/* Ensure node is writable */
	if (_dom_node_readonly(node)) {
		return DOM_NO_MODIFICATION_ALLOWED_ERR;
	}

	/* No longer want existing prefix */
	if (node->prefix != NULL) {
		dom_string_unref(node->prefix);
	}

	/* Set the prefix */
	if (prefix != NULL) {
		/* Empty string is treated as NULL */
		if (dom_string_length(prefix) == 0) {
			node->prefix = NULL;
		} else {
			node->prefix = dom_string_ref(prefix);
		}
	} else {
		node->prefix = NULL;
	}

	return DOM_NO_ERR;
}

/**
 * Retrieve the local part of a node's qualified name
 *
 * \param node    The node to retrieve the local name of
 * \param result  Pointer to location to receive local name
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception _dom_node_get_local_name(dom_node_internal *node,
		dom_string **result)
{	
	assert(node->owner != NULL);
	
	/* Only Element and Attribute nodes may have a local name */
	if (node->type != DOM_ELEMENT_NODE && 
			node->type != DOM_ATTRIBUTE_NODE) {
		*result = NULL;
		return DOM_NO_ERR;
	}

	/* The node may have a local name, reference it if so */
	if (node->name != NULL)
		*result = dom_string_ref(node->name);
	else
		*result = NULL;

	return DOM_NO_ERR;
}

/**
 * Determine if a node has any attributes
 *
 * \param node    Node to inspect
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_node_has_attributes(dom_node_internal *node, bool *result)
{
	UNUSED(node);
	*result = false;

	return DOM_NO_ERR;
}

/**
 * Retrieve the base URI of a DOM node
 *
 * \param node    The node to retrieve the base URI of
 * \param result  Pointer to location to receive base URI
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 *
 * We don't support this API now, so this function call should always
 * return DOM_NOT_SUPPORTED_ERR.
 */
dom_exception _dom_node_get_base(dom_node_internal *node,
		dom_string **result)
{
	struct dom_document *doc = node->owner;
	assert(doc != NULL);

	return dom_document_get_base(doc, result);
}

/**
 * Compare the positions of two nodes in a DOM tree
 *
 * \param node   The reference node
 * \param other  The node to compare
 * \param result Pointer to location to receive result
 * \return DOM_NO_ERR            on success,
 *         DOM_NOT_SUPPORTED_ERR when the nodes are from different DOM
 *                               implementations.
 *
 * The result is a bitfield of dom_document_position values.
 *
 * We don't support this API now, so this function call should always
 * return DOM_NOT_SUPPORTED_ERR.
 */
dom_exception _dom_node_compare_document_position(dom_node_internal *node,
		dom_node_internal *other, uint16_t *result)
{
	UNUSED(node);
	UNUSED(other);
	UNUSED(result);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Retrieve the text content of a DOM node
 *
 * \param node    The node to retrieve the text content of
 * \param result  Pointer to location to receive text content
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 *
 * If there is no text content in the code, NULL will returned in \a result.
 *
 * DOM3Core states that this can raise DOMSTRING_SIZE_ERR. It will not in
 * this implementation; dom_strings are unbounded.
 */
dom_exception _dom_node_get_text_content(dom_node_internal *node,
		dom_string **result)
{
	dom_node_internal *n;
	dom_string *str = NULL;
	dom_string *ret = NULL;

	assert(node->owner != NULL);
	
	for (n = node->first_child; n != NULL; n = n->next) {
		if (n->type == DOM_COMMENT_NODE ||
		    n->type == DOM_PROCESSING_INSTRUCTION_NODE)
			continue;
		dom_node_get_text_content(n, (str == NULL) ? &str : &ret);
		if (ret != NULL) {
			dom_string *new_str;
			dom_string_concat(str, ret, &new_str);
			dom_string_unref(str);
			dom_string_unref(ret);
			str = new_str;
		}
	}
	
	*result = str;

	return DOM_NO_ERR;
}

/**
 * Set the text content of a DOM node
 *
 * \param node     The node to set the text content of
 * \param content  New text content for node
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::node is readonly.
 *
 * Any child nodes ::node may have are removed and replaced with a single
 * Text node containing the new content.
 */
dom_exception _dom_node_set_text_content(dom_node_internal *node,
		dom_string *content)
{
	dom_node_internal *n, *p, *r;
	dom_document *doc;
	dom_text *text;
	dom_exception err;

	n = node->first_child;

	while (n != NULL) {
		p = n;
		n = n->next;
		/* Add the (void *) casting to avoid gcc warning:
		 * dereferencing type-punned pointer will break 
		 * strict-aliasing rules */
		err = dom_node_remove_child(node, p, (void *) &r);
		if (err != DOM_NO_ERR)
			return err;
	}

	doc = node->owner;
	assert(doc != NULL);

	err = dom_document_create_text_node(doc, content, &text);
	if (err != DOM_NO_ERR)
		return err;
	
	err = dom_node_append_child(node, text, (void *) &r);
	if (err != DOM_NO_ERR)
		return err;

	return DOM_NO_ERR;
}

/**
 * Determine if two DOM nodes are the same
 *
 * \param node    The node to compare
 * \param other   The node to compare against
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * This tests if the two nodes reference the same object.
 */
dom_exception _dom_node_is_same(dom_node_internal *node,
		dom_node_internal *other, bool *result)
{
	*result = (node == other);

	return DOM_NO_ERR;
}

/**
 * Lookup the prefix associated with the given namespace URI
 *
 * \param node       The node to start prefix search from
 * \param namespace  The namespace URI
 * \param result     Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception _dom_node_lookup_prefix(dom_node_internal *node,
		dom_string *namespace, dom_string **result)
{
	if (node->parent != NULL)
		return dom_node_lookup_prefix(node, namespace, result);
	else
		*result = NULL;

	return DOM_NO_ERR;
}

/**
 * Determine if the specified namespace is the default namespace
 *
 * \param node       The node to query
 * \param namespace  The namespace URI to test
 * \param result     Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_node_is_default_namespace(dom_node_internal *node,
		dom_string *namespace, bool *result)
{
	if (node->parent != NULL)
		return dom_node_is_default_namespace(node, namespace, result);
	else
		*result = false;
	return DOM_NO_ERR;
}

/**
 * Lookup the namespace URI associated with the given prefix
 *
 * \param node    The node to start namespace search from
 * \param prefix  The prefix to look for, or NULL to find default.
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception _dom_node_lookup_namespace(dom_node_internal *node,
		dom_string *prefix, dom_string **result)
{
	if (node->parent != NULL)
		return dom_node_lookup_namespace(node->parent, prefix, result);
	else
		*result = NULL;

	return DOM_NO_ERR;
}

/**
 * Determine if two DOM nodes are equal
 *
 * \param node    The node to compare
 * \param other   The node to compare against
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * Two nodes are equal iff:
 *   + They are of the same type
 *   + nodeName, localName, namespaceURI, prefix, nodeValue are equal
 *   + The node attributes are equal
 *   + The child nodes are equal
 *
 * Two DocumentType nodes are equal iff:
 *   + publicId, systemId, internalSubset are equal
 *   + The node entities are equal
 *   + The node notations are equal
 * TODO: in document_type, we should override this virtual function
 * TODO: actually handle DocumentType nodes differently
 */
dom_exception _dom_node_is_equal(dom_node_internal *node,
		dom_node_internal *other, bool *result)
{
	dom_exception err = DOM_NO_ERR;
	dom_namednodemap *m1 = NULL, *m2 = NULL;
	dom_nodelist *l1 = NULL, *l2 = NULL;
	*result = false;

	/* Compare the node types */
	if (node->type != other->type){
		/* different */
		err = DOM_NO_ERR;
		goto cleanup;
	}

	assert(node->owner != NULL);
	assert(other->owner != NULL);

	/* Compare node name */
	if (dom_string_isequal(node->name, other->name) == false) {
		/* different */
		goto cleanup;
	}

	/* Compare prefix */
	if (dom_string_isequal(node->prefix, other->prefix) == false) {
		/* different */
		goto cleanup;
	}

	/* Compare namespace URI */
	if (dom_string_isequal(node->namespace, other->namespace) == false) {
		/* different */
		goto cleanup;
	}

	/* Compare node value */
	if (dom_string_isequal(node->value, other->value) == false) {
		/* different */
		goto cleanup;
	}

	/* Compare the attributes */
	err = dom_node_get_attributes(node, &m1);
	if (err != DOM_NO_ERR) {
		/* error */
		goto cleanup;
	}
	
	err = dom_node_get_attributes(other, &m2);
	if (err != DOM_NO_ERR) {
		/* error */
		goto cleanup;
	}

	if (dom_namednodemap_equal(m1, m2) == false) {
		/* different */
		goto cleanup;
	}

	/* Compare the children */
	err = dom_node_get_child_nodes(node, &l1);
	if (err != DOM_NO_ERR) {
		/* error */
		goto cleanup;
	}

	err = dom_node_get_child_nodes(other, &l2);
	if (err != DOM_NO_ERR) {
		/* error */
		goto cleanup;
	}

	if (dom_nodelist_equal(l1, l2) == false) {
		/* different */
		goto cleanup;
	}

	*result = true;

cleanup:
	if (m1 != NULL)
		dom_namednodemap_unref(m1);
	if (m2 != NULL)
		dom_namednodemap_unref(m2);

	if (l1 != NULL)
		dom_nodelist_unref(l1);
	if (l2 != NULL)
		dom_nodelist_unref(l2);

	return err;
}

/**
 * Retrieve an object which implements the specialized APIs of the specified
 * feature and version.
 *
 * \param node     The node to query
 * \param feature  The requested feature
 * \param version  The version number of the feature
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_node_get_feature(dom_node_internal *node,
		dom_string *feature, dom_string *version,
		void **result)
{
	bool has;

	dom_implementation_has_feature(dom_string_data(feature), 
			dom_string_data(version), &has);

	if (has) {
		*result = node;
	} else {
		*result = NULL;
	}

	return DOM_NO_ERR;
}

/**
 * Associate an object to a key on this node
 *
 * \param node     The node to insert object into
 * \param key      The key associated with the object
 * \param data     The object to associate with key, or NULL to remove
 * \param handler  User handler function, or NULL if none
 * \param result   Pointer to location to receive previously associated object
 * \return DOM_NO_ERR.
 */
dom_exception _dom_node_set_user_data(dom_node_internal *node,
		dom_string *key, void *data,
		dom_user_data_handler handler, void **result)
{
	struct dom_user_data *ud = NULL;
	void *prevdata = NULL;

	/* Search for user data */
	for (ud = node->user_data; ud != NULL; ud = ud->next) {
		if (dom_string_isequal(ud->key, key))
			break;
	};

	/* Remove it, if found and no new data */
	if (data == NULL && ud != NULL) {
		dom_string_unref(ud->key);

		if (ud->next != NULL)
			ud->next->prev = ud->prev;
		if (ud->prev != NULL)
			ud->prev->next = ud->next;
		else
			node->user_data = ud->next;

		*result = ud->data;

		free(ud);

		return DOM_NO_ERR;
	}

	/* Otherwise, create a new user data object if one wasn't found */
	if (ud == NULL) {
		ud = malloc(sizeof(struct dom_user_data));
		if (ud == NULL)
			return DOM_NO_MEM_ERR;

		dom_string_ref(key);
		ud->key = key;
		ud->data = NULL;
		ud->handler = NULL;

		/* Insert into list */
		ud->prev = NULL;
		ud->next = node->user_data;
		if (node->user_data)
			node->user_data->prev = ud;
		node->user_data = ud;
	}

	prevdata = ud->data;

	/* And associate data with it */
	ud->data = data;
	ud->handler = handler;

	*result = prevdata;

	return DOM_NO_ERR;
}

/**
 * Retrieves the object associated to a key on this node
 *
 * \param node    The node to retrieve object from
 * \param key     The key to search for
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_node_get_user_data(dom_node_internal *node,
		dom_string *key, void **result)
{
	struct dom_user_data *ud = NULL;

	/* Search for user data */
	for (ud = node->user_data; ud != NULL; ud = ud->next) {
		if (dom_string_isequal(ud->key, key))
			break;
	};

	if (ud != NULL)
		*result = ud->data;
	else
		*result = NULL;

	return DOM_NO_ERR;
}


/*--------------------------------------------------------------------------*/

/* The protected virtual functions */

/* Copy the internal attributes of a Node from old to new */
dom_exception _dom_node_copy(dom_node_internal *old, dom_node_internal **copy)
{
	dom_node_internal *new_node;
	dom_exception err;

	new_node = malloc(sizeof(dom_node_internal));
	if (new_node == NULL)
		return DOM_NO_MEM_ERR;

	err = _dom_node_copy_internal(old, new_node);
	if (err != DOM_NO_ERR) {
		free(new_node);
		return err;
	}

	*copy = new_node;

	return DOM_NO_ERR;
}

dom_exception _dom_node_copy_internal(dom_node_internal *old, 
		dom_node_internal *new)
{
	new->base.vtable = old->base.vtable;
	new->vtable = old->vtable;

	new->name = dom_string_ref(old->name);

	/* Value - see below */

	new->type = old->type;
	new->parent = NULL;
	new->first_child = NULL;
	new->last_child = NULL;
	new->previous = NULL;
	new->next = NULL;

	assert(old->owner != NULL);

	new->owner = old->owner;

	if (old->namespace != NULL)
		new->namespace = dom_string_ref(old->namespace);
	else
		new->namespace = NULL;

	if (old->prefix != NULL)
		new->prefix = dom_string_ref(old->prefix);
	else
		new->prefix = NULL;

	new->user_data = NULL;
	new->base.refcnt = 1;

	list_init(&new->pending_list);

	/* Value */	
	if (old->value != NULL) {
		dom_string_ref(old->value);

		new->value = old->value;
	} else {
		new->value = NULL;
	}
	
	/* The new copyed node has no parent, 
	 * so it should be put in the pending list. */
	dom_node_mark_pending(new);

	/* Intialise the EventTarget interface */
	return _dom_event_target_internal_initialise(&new->eti);
}


/*--------------------------------------------------------------------------*/

/*  The helper functions */

/**
 * Determine if a node is permitted as a child of another node
 *
 * \param parent  Prospective parent
 * \param child   Prospective child
 * \return true if ::child is permitted as a child of ::parent, false otherwise.
 */
bool _dom_node_permitted_child(const dom_node_internal *parent, 
		const dom_node_internal *child)
{
	bool valid = false;

	/* See DOM3Core $1.1.1 for details */

	switch (parent->type) {
	case DOM_ELEMENT_NODE:
	case DOM_ENTITY_REFERENCE_NODE:
	case DOM_ENTITY_NODE:
	case DOM_DOCUMENT_FRAGMENT_NODE:
		valid = (child->type == DOM_ELEMENT_NODE || 
			 child->type == DOM_TEXT_NODE || 
			 child->type == DOM_COMMENT_NODE || 
			 child->type == DOM_PROCESSING_INSTRUCTION_NODE || 
			 child->type == DOM_CDATA_SECTION_NODE || 
			 child->type == DOM_ENTITY_REFERENCE_NODE);
		break;

	case DOM_ATTRIBUTE_NODE:
		valid = (child->type == DOM_TEXT_NODE ||
			 child->type == DOM_ENTITY_REFERENCE_NODE);
		break;

	case DOM_TEXT_NODE:
	case DOM_CDATA_SECTION_NODE:
	case DOM_PROCESSING_INSTRUCTION_NODE:
	case DOM_COMMENT_NODE:
	case DOM_DOCUMENT_TYPE_NODE:
	case DOM_NOTATION_NODE:
		valid = false;
		break;

	case DOM_DOCUMENT_NODE:
		valid = (child->type == DOM_ELEMENT_NODE ||
			 child->type == DOM_PROCESSING_INSTRUCTION_NODE ||
			 child->type == DOM_COMMENT_NODE ||
			 child->type == DOM_DOCUMENT_TYPE_NODE);

		/* Ensure that the document doesn't already 
		 * have a root element */
		if (child->type == DOM_ELEMENT_NODE) {
			dom_node_internal *n;
			for (n = parent->first_child; 
					n != NULL; n = n->next) {
				if (n->type == DOM_ELEMENT_NODE)
					valid = false;
			}
		}

		/* Ensure that the document doesn't already 
		 * have a document type */
		if (child->type == DOM_DOCUMENT_TYPE_NODE) {
			dom_node_internal *n;
			for (n = parent->first_child;
					n != NULL; n = n->next) {
				if (n->type == DOM_DOCUMENT_TYPE_NODE)
					valid = false;
			}
		}

		break;
	}

	return valid;
}

/**
 * Determine if a node is read only
 *
 * \param node  The node to consider
 */
bool _dom_node_readonly(const dom_node_internal *node)
{
	const dom_node_internal *n = node;

	/* DocumentType and Notation ns are read only */
	if (n->type == DOM_DOCUMENT_TYPE_NODE ||
			n->type == DOM_NOTATION_NODE)
		return true;
	
	/* Some Attr node are readonly */
	if (n->type == DOM_ATTRIBUTE_NODE)
		return _dom_attr_readonly((const dom_attr *) n);

	/* Entity ns and their descendants are read only 
	 * EntityReference ns and their descendants are read only */
	for (n = node; n != NULL; n = n->parent) {
		if (n->type == DOM_ENTITY_NODE
				|| n->type == DOM_ENTITY_REFERENCE_NODE)
			return true;
	}

	/* Otherwise, it's writable */
	return false;
}

/**
 * Attach a node to the tree
 *
 * \param node      The node to attach
 * \param parent    Node to attach ::node as child of
 * \param previous  Previous node in sibling list, or NULL if none
 * \param next      Next node in sibling list, or NULL if none
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_node_attach(dom_node_internal *node,
		dom_node_internal *parent, dom_node_internal *previous,
		dom_node_internal *next)
{
	return _dom_node_attach_range(node, node, parent, previous, next);
}

/**
 * Detach a node from the tree
 *
 * \param node  The node to detach
 */
void _dom_node_detach(dom_node_internal *node)
{
	/* When a Node is not in the document tree, it must be in the 
	 * pending list */
	dom_node_mark_pending(node);

	_dom_node_detach_range(node, node);
}

/**
 * Attach a range of nodes to the tree
 *
 * \param first     First node in the range
 * \param last      Last node in the range
 * \param parent    Node to attach range to
 * \param previous  Previous node in sibling list, or NULL if none
 * \param next      Next node in sibling list, or NULL if none
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 *
 * The range is assumed to be a linked list of sibling nodes.
 */
dom_exception _dom_node_attach_range(dom_node_internal *first, 
		dom_node_internal *last,
		dom_node_internal *parent, 
		dom_node_internal *previous, 
		dom_node_internal *next)
{
	dom_exception err;
	bool success = true;
	dom_node_internal *n;

	first->previous = previous;
	last->next = next;

	if (previous != NULL)
		previous->next = first;
	else
		parent->first_child = first;

	if (next != NULL)
		next->previous = last;
	else
		parent->last_child = last;

	for (n = first; n != last->next; n = n->next) {
		n->parent = parent;
		/* Dispatch a DOMNodeInserted event */
		err = dom_node_dispatch_node_change_event(parent->owner, 
				n, parent, DOM_MUTATION_ADDITION, &success);
		if (err != DOM_NO_ERR)
			return err;
	}

	success = true;
	err = _dom_dispatch_subtree_modified_event(parent->owner, parent,
			&success);
	if (err != DOM_NO_ERR)
		return err;

	return DOM_NO_ERR;
}

/**
 * Detach a range of nodes from the tree
 *
 * \param first  The first node in the range
 * \param last   The last node in the range
 *
 * The range is assumed to be a linked list of sibling nodes.
 */
dom_exception _dom_node_detach_range(dom_node_internal *first,
		dom_node_internal *last)
{
	bool success = true;
	dom_node_internal *parent;
	dom_node_internal *n;
	dom_exception err = DOM_NO_ERR;

	if (first->previous != NULL)
		first->previous->next = last->next;
	else
		first->parent->first_child = last->next;

	if (last->next != NULL)
		last->next->previous = first->previous;
	else
		last->parent->last_child = first->previous;

	parent = first->parent;
	for (n = first; n != last->next; n = n->next) {
		/* Dispatch a DOMNodeRemoval event */
		err = dom_node_dispatch_node_change_event(n->owner, n,
				n->parent, DOM_MUTATION_REMOVAL, &success);

		n->parent = NULL;
	}

	success = true;
	_dom_dispatch_subtree_modified_event(parent->owner, parent,
			&success);

	first->previous = NULL;
	last->next = NULL;

	return err;
}

/**
 * Replace a node in the tree
 *
 * \param old          Node to replace
 * \param replacement  Replacement node
 *
 * This is not implemented in terms of attach/detach in case 
 * we want to perform any special replacement-related behaviour 
 * at a later date.
 */
void _dom_node_replace(dom_node_internal *old,
		dom_node_internal *replacement)
{
	dom_node_internal *first, *last;
	dom_node_internal *n;

	if (replacement->type == DOM_DOCUMENT_FRAGMENT_NODE) {
		first = replacement->first_child;
		last = replacement->last_child;

		replacement->first_child = replacement->last_child = NULL;
	} else {
		first = replacement;
		last = replacement;
	}

	first->previous = old->previous;
	last->next = old->next;

	if (old->previous != NULL)
		old->previous->next = first;
	else
		old->parent->first_child = first;

	if (old->next != NULL)
		old->next->previous = last;
	else
		old->parent->last_child = last;

	for (n = first; n != last->next; n = n->next) {
		n->parent = old->parent;
	}

	old->previous = old->next = old->parent = NULL;
}

/**
 * Merge two adjacent text nodes into one text node.
 *
 * \param p  The first text node
 * \param n  The second text node
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_merge_adjacent_text(dom_node_internal *p,
		dom_node_internal *n)
{
	dom_string *str;
	dom_exception err;

	assert(p->type == DOM_TEXT_NODE);
	assert(n->type == DOM_TEXT_NODE);

	err = dom_text_get_whole_text(n, &str);
	if (err != DOM_NO_ERR)
		return err;
	
	err = dom_characterdata_append_data(p, str);
	if (err != DOM_NO_ERR)
		return err;

	dom_string_unref(str);

	return DOM_NO_ERR;
}

/**
 * Try to destroy this node. 
 * 
 * \param node	The node to destroy
 *
 * When some node owns this node, (such as an elment owns its attribute nodes)
 * when this node being not owned, the owner should call this function to try
 * to destroy this node. 
 *
 * @note: Owning a node does not means this node's refcnt is above zero.
 */
dom_exception _dom_node_try_destroy(dom_node_internal *node)
{
	if (node == NULL)
		return DOM_NO_ERR;

	if (node->parent == NULL) {
		if (node->base.refcnt == 0) {
			dom_node_destroy(node);
		} else if (node->pending_list.prev == &node->pending_list){
			assert (node->pending_list.next == &node->pending_list);
			list_append(&node->owner->pending_nodes,
					&node->pending_list);
		}
	}
        
        return DOM_NO_ERR;
}

/**
 * To add some node to the pending list, when a node is removed from its parent
 * or an attribute is removed from its element
 *
 * \param node  The Node instance
 */
void _dom_node_mark_pending(dom_node_internal *node)
{
	struct dom_document *doc = node->owner;

	/* TODO: the pending_list is located at in dom_document, but some
	 * nodes can be created without a document created, such as a 
	 * dom_document_type node. For this reason, we should test whether
	 * the doc is NULL. */ 
	if (doc != NULL) {
		/* The node must not be in the pending list */
		assert(node->pending_list.prev == &node->pending_list);

		list_append(&doc->pending_nodes, &node->pending_list);
	}
}

/**
 * To remove the node from the pending list, this may happen when
 * a node is removed and then appended to another parent
 *
 * \param node  The Node instance
 */
void _dom_node_remove_pending(dom_node_internal *node)
{
	struct dom_document *doc = node->owner;

	if (doc != NULL) {
		/* The node must be in the pending list */
		assert(node->pending_list.prev != &node->pending_list);

		list_del(&node->pending_list);
	}
}

/******************************************************************************
 * Event Target API                                                           *
 ******************************************************************************/

dom_exception _dom_node_add_event_listener(dom_event_target *et,
		dom_string *type, struct dom_event_listener *listener, 
		bool capture)
{
	dom_node_internal *node = (dom_node_internal *) et;

	return _dom_event_target_add_event_listener(&node->eti, type, 
			listener, capture);
}

dom_exception _dom_node_remove_event_listener(dom_event_target *et,
		dom_string *type, struct dom_event_listener *listener, 
		bool capture)
{
	dom_node_internal *node = (dom_node_internal *) et;

	return _dom_event_target_remove_event_listener(&node->eti,
			type, listener, capture);
}

dom_exception _dom_node_add_event_listener_ns(dom_event_target *et,
		dom_string *namespace, dom_string *type, 
		struct dom_event_listener *listener, bool capture)
{
	dom_node_internal *node = (dom_node_internal *) et;

	return _dom_event_target_add_event_listener_ns(&node->eti,
			namespace, type, listener, capture);
}

dom_exception _dom_node_remove_event_listener_ns(dom_event_target *et,
		dom_string *namespace, dom_string *type, 
		struct dom_event_listener *listener, bool capture)
{
	dom_node_internal *node = (dom_node_internal *) et;

	return _dom_event_target_remove_event_listener_ns(&node->eti,
			namespace, type, listener, capture);
}

/**
 * Dispatch an event into the implementation's event model
 *
 * \param et       The EventTarget object
 * \param eti      Internal EventTarget
 * \param evt      The event object
 * \param success  Indicates whether any of the listeners which handled the 
 *                 event called Event.preventDefault(). If 
 *                 Event.preventDefault() was called the returned value is 
 *                 false, else it is true.
 * \return DOM_NO_ERR                     on success
 *         DOM_DISPATCH_REQUEST_ERR       If the event is already in dispatch
 *         DOM_UNSPECIFIED_EVENT_TYPE_ERR If the type of the event is Null or
 *                                        empty string.
 *         DOM_NOT_SUPPORTED_ERR          If the event is not created by 
 *                                        Document.createEvent
 *         DOM_INVALID_CHARACTER_ERR      If the type of this event is not a
 *                                        valid NCName.
 */
dom_exception _dom_node_dispatch_event(dom_event_target *et,
		struct dom_event *evt, bool *success)
{
	dom_exception err, ret = DOM_NO_ERR;
	dom_node_internal *target = (dom_node_internal *) et;
	dom_document *doc;
	dom_document_event_internal *dei;
	dom_event_target **targets;
	uint32_t ntargets, ntargets_allocated, targetnr;
	void *pw;

	assert(et != NULL);
	assert(evt != NULL);

	/* To test whether this event is in dispatch */
	if (evt->in_dispatch == true) {
		return DOM_DISPATCH_REQUEST_ERR;
	} else {
		evt->in_dispatch = true;
	}

	if (evt->type == NULL || dom_string_byte_length(evt->type) == 0) {
		return DOM_UNSPECIFIED_EVENT_TYPE_ERR;
	}

	if (evt->doc == NULL)
		return DOM_NOT_SUPPORTED_ERR;
	
	doc = dom_node_get_owner(et);
	if (doc == NULL) {
		/* TODO: In the progress of parsing, many Nodes in the DTD has
		 * no document at all, do nothing for this kind of node */
		return DOM_NO_ERR;
	}
	
	*success = true;

	/* Compose the event target list */
	ntargets = 0;
	ntargets_allocated = 64;
	targets = calloc(sizeof(*targets), ntargets_allocated);
	if (targets == NULL) {
		/** \todo Report memory exhaustion? */
		return DOM_NO_ERR;
	}
	targets[ntargets++] = (dom_event_target *)dom_node_ref(et);
	target = target->parent;

	while (target != NULL) {
		if (ntargets == ntargets_allocated) {
			dom_event_target **newtargets = realloc(
				targets,
				ntargets_allocated * 2 * sizeof(*targets));
			if (newtargets == NULL)
				goto cleanup;
			memset(newtargets + ntargets_allocated,
			       0, ntargets_allocated * sizeof(*newtargets));
			targets = newtargets;
			ntargets_allocated *= 2;
		}
		targets[ntargets++] = (dom_event_target *)dom_node_ref(target);
		target = target->parent;
	}

	/* Fill the target of the event */
	evt->target = et;
	evt->phase = DOM_CAPTURING_PHASE;

	/* The started callback of default action */
	dei = &doc->dei;
	pw = dei->actions_ctx;
	if (dei->actions != NULL) {
		dom_default_action_callback cb = dei->actions(evt->type,
				DOM_DEFAULT_ACTION_STARTED, &pw);
		if (cb != NULL) {
			cb(evt, pw);
		}
	}

	/* The capture phase */
	for (targetnr = ntargets; targetnr > 0; --targetnr) {
		dom_node_internal *node =
			(dom_node_internal *) targets[targetnr - 1];

		err = _dom_event_target_dispatch(targets[targetnr - 1],
				&node->eti, evt, DOM_CAPTURING_PHASE, success);
		if (err != DOM_NO_ERR) {
			ret = err;
			goto cleanup;
		}
		/* If the stopImmediatePropagation or stopPropagation is
		 * called, we should break */
		if (evt->stop_now == true || evt->stop == true)
			goto cleanup;
	}

	/* Target phase */
	evt->phase = DOM_AT_TARGET;
	evt->current = et;
	err = _dom_event_target_dispatch(et, &((dom_node_internal *) et)->eti,
			evt, DOM_AT_TARGET, success);
	if (err != DOM_NO_ERR) {
		ret = err;
		goto cleanup;
	}
	if (evt->stop_now == true || evt->stop == true)
		goto cleanup;

	/* Bubbling phase */
	evt->phase = DOM_BUBBLING_PHASE;

	for (targetnr = 0; targetnr < ntargets; ++targetnr) {
		dom_node_internal *node =
			(dom_node_internal *) targets[targetnr];
		err = _dom_event_target_dispatch(targets[targetnr],
				&node->eti, evt, DOM_BUBBLING_PHASE, success);
		if (err != DOM_NO_ERR) {
			ret = err;
			goto cleanup;
		}
		/* If the stopImmediatePropagation or stopPropagation is
		 * called, we should break */
		if (evt->stop_now == true || evt->stop == true)
			goto cleanup;
	}

	if (dei->actions == NULL)
		goto cleanup;

	/* The end callback of default action */
	if (evt->prevent_default != true) {
		dom_default_action_callback cb = dei->actions(evt->type,
				DOM_DEFAULT_ACTION_END, &pw);
		if (cb != NULL) {
			cb(evt, pw);
		}
	}

	/* The prevented callback of default action */
	if (evt->prevent_default != true) {
		dom_default_action_callback cb = dei->actions(evt->type,
				DOM_DEFAULT_ACTION_PREVENTED, &pw);
		if (cb != NULL) {
			cb(evt, pw);
		}
	}

cleanup:
	if (evt->prevent_default == true) {
		*success = false;
	}

	while (ntargets--) {
		dom_node_unref(targets[ntargets]);
	}
	free(targets);

	return ret;
}

dom_exception _dom_node_dispatch_node_change_event(dom_document *doc,
		dom_node_internal *node, dom_node_internal *related,
		dom_mutation_type change, bool *success)
{
	dom_node_internal *target;
	dom_exception err;

	/* Fire change event at immediate target */
	err = _dom_dispatch_node_change_event(doc, node, related, 
			change, success);
	if (err != DOM_NO_ERR)
		return err;

	/* Fire document change event at subtree */
	target = node->first_child;
	while (target != NULL) {
		err = _dom_dispatch_node_change_document_event(doc, target, 
				change, success);
		if (err != DOM_NO_ERR)
			return err;

		if (target->first_child != NULL) {
			target = target->first_child;
		} else if (target->next != NULL) {
			target = target->next;
		} else {
			dom_node_internal *parent = target->parent;

			while (parent != node && target == parent->last_child) {
				target = parent;
				parent = target->parent;
			}

			target = target->next;
		}
	}

	return DOM_NO_ERR;
}

