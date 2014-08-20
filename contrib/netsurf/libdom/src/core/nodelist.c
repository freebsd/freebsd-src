/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdlib.h>

#include <dom/core/node.h>
#include <dom/core/document.h>
#include <dom/core/nodelist.h>
#include <dom/core/string.h>

#include "core/document.h"
#include "core/node.h"
#include "core/nodelist.h"

#include "utils/utils.h"

/**
 * DOM node list
 */
struct dom_nodelist {
	dom_document *owner;	/**< Owning document */

	dom_node_internal *root;	
			/**< Root of applicable subtree */

	nodelist_type type;	/**< Type of this list */

	union {
		struct {
			dom_string *name;
					/**< Tag name to match */
			bool any_name;		/**< The name is '*' */
		} n;
		struct {
			bool any_namespace;	/**< The namespace is '*' */
			bool any_localname;	/**< The localname is '*' */
			dom_string *namespace;	/**< Namespace */
			dom_string *localname;	/**< Localname */
		} ns;			/**< Data for namespace matching */
	} data;

	uint32_t refcnt;		/**< Reference count */
};

/**
 * Create a nodelist
 *
 * \param doc        Owning document
 * \param type	     The type of the NodeList
 * \param root       Root node of subtree that list applies to
 * \param tagname    Name of nodes in list (or NULL)
 * \param namespace  Namespace part of nodes in list (or NULL)
 * \param localname  Local part of nodes in list (or NULL)
 * \param list       Pointer to location to receive list
 * \return DOM_NO_ERR on success, DOM_NO_MEM_ERR on memory exhaustion
 *
 * ::root must be a node owned by ::doc
 *
 * The returned list will already be referenced, so the client need not
 * do so explicitly. The client must unref the list once finished with it.
 */
dom_exception _dom_nodelist_create(dom_document *doc, nodelist_type type,
		dom_node_internal *root, dom_string *tagname,
		dom_string *namespace, dom_string *localname,
		dom_nodelist **list)
{
	dom_nodelist *l;

	l = malloc(sizeof(dom_nodelist));
	if (l == NULL)
		return DOM_NO_MEM_ERR;

	dom_node_ref(doc);
	l->owner = doc;

	dom_node_ref(root);
	l->root = root;

	l->type = type;

	if (type == DOM_NODELIST_BY_NAME || 
	    type == DOM_NODELIST_BY_NAME_CASELESS) {
		assert(tagname != NULL);
		l->data.n.any_name = false;
		if (dom_string_byte_length(tagname) == 1) {
			const char *ch = dom_string_data(tagname);
			if (*ch == '*') {
				l->data.n.any_name = true;
			}
		}
	
		l->data.n.name = dom_string_ref(tagname);
	} else if (type == DOM_NODELIST_BY_NAMESPACE ||
		   type == DOM_NODELIST_BY_NAMESPACE_CASELESS) {
		l->data.ns.any_localname = false;
		l->data.ns.any_namespace = false;
		if (localname != NULL) {
			if (dom_string_byte_length(localname) == 1) {
				const char *ch = dom_string_data(localname);
				if (*ch == '*') {
				   l->data.ns.any_localname = true;
				}
			}
			dom_string_ref(localname);
		}
		if (namespace != NULL) {
			if (dom_string_byte_length(namespace) == 1) {
				const char *ch = dom_string_data(namespace);
				if (*ch == '*') {
					l->data.ns.any_namespace = true;
				}
			}
			dom_string_ref(namespace);
		}

		l->data.ns.namespace = namespace;
		l->data.ns.localname = localname;
	} 

	l->refcnt = 1;

	*list = l;

	return DOM_NO_ERR;
}

/**
 * Claim a reference on a DOM node list
 *
 * \param list  The list to claim a reference on
 */
void dom_nodelist_ref(dom_nodelist *list)
{
	assert(list != NULL);
	list->refcnt++;
}

/**
 * Release a reference on a DOM node list
 *
 * \param list  The list to release the reference from
 *
 * If the reference count reaches zero, any memory claimed by the
 * list will be released
 */
void dom_nodelist_unref(dom_nodelist *list)
{
	if (list == NULL)
		return;

	if (--list->refcnt == 0) {
		dom_node_internal *owner = (dom_node_internal *) list->owner;
		switch (list->type) {
		case DOM_NODELIST_CHILDREN:
			/* Nothing to do */
			break;
		case DOM_NODELIST_BY_NAMESPACE:
		case DOM_NODELIST_BY_NAMESPACE_CASELESS:
			if (list->data.ns.namespace != NULL)
				dom_string_unref(list->data.ns.namespace);
			if (list->data.ns.localname != NULL)
				dom_string_unref(list->data.ns.localname);
			break;
		case DOM_NODELIST_BY_NAME:
		case DOM_NODELIST_BY_NAME_CASELESS:
			assert(list->data.n.name != NULL);
			dom_string_unref(list->data.n.name);
			break;
		}

		dom_node_unref(list->root);

		/* Remove list from document */
		_dom_document_remove_nodelist(list->owner, list);

		/* Destroy the list object */
		free(list);

		/* And release our reference on the owning document
		 * This must be last as, otherwise, it's possible that
		 * the document is destroyed before we are */
		dom_node_unref(owner);
	}
}

/**
 * Retrieve the length of a node list
 *
 * \param list    List to retrieve length of
 * \param length  Pointer to location to receive length
 * \return DOM_NO_ERR.
 */
dom_exception dom_nodelist_get_length(dom_nodelist *list, uint32_t *length)
{
	dom_node_internal *cur = list->root->first_child;
	uint32_t len = 0;

	/* Traverse data structure */
	while (cur != NULL) {
		/* Process current node */
		if (list->type == DOM_NODELIST_CHILDREN) {
			len++;
		} else if (list->type == DOM_NODELIST_BY_NAME) {
			if (list->data.n.any_name == true || (
					cur->name != NULL && 
					dom_string_isequal(cur->name, 
						list->data.n.name))) {
				if (cur->type == DOM_ELEMENT_NODE)
					len++;
			}
		} else if (list->type == DOM_NODELIST_BY_NAME_CASELESS) {
			if (list->data.n.any_name == true || (
					cur->name != NULL && 
					dom_string_caseless_isequal(cur->name, 
						list->data.n.name))) {
				if (cur->type == DOM_ELEMENT_NODE)
					len++;
			}
		} else if (list->type == DOM_NODELIST_BY_NAMESPACE) {
			if (list->data.ns.any_namespace == true ||
					dom_string_isequal(cur->namespace,
					list->data.ns.namespace)) {
				if (list->data.ns.any_localname == true ||
						(cur->name != NULL &&
						dom_string_isequal(cur->name,
						list->data.ns.localname))) {
					if (cur->type == DOM_ELEMENT_NODE)
						len++;
				}
			}
		} else if (list->type == DOM_NODELIST_BY_NAMESPACE_CASELESS) {
			if (list->data.ns.any_namespace == true ||
					dom_string_caseless_isequal(
					cur->namespace,
					list->data.ns.namespace)) {
				if (list->data.ns.any_localname == true ||
						(cur->name != NULL &&
						dom_string_caseless_isequal(
						cur->name,
						list->data.ns.localname))) {
					if (cur->type == DOM_ELEMENT_NODE)
						len++;
				}
			}
		} else {
			assert("Unknown list type" == NULL);
		}

		/* Now, find next node */
		if (list->type == DOM_NODELIST_CHILDREN) {
			/* Just interested in sibling list */
			cur = cur->next;
		} else {
			/* Want a full in-order tree traversal */
			if (cur->first_child != NULL) {
				/* Has children */
				cur = cur->first_child;
			} else if (cur->next != NULL) {
				/* No children, but has siblings */
				cur = cur->next;
			} else {
				/* No children or siblings. 
				 * Find first unvisited relation. */
				dom_node_internal *parent = cur->parent;

				while (parent != list->root &&
						cur == parent->last_child) {
					cur = parent;
					parent = parent->parent;
				}

				cur = cur->next;
			}
		}
	}

	*length = len;

	return DOM_NO_ERR;
}

/**
 * Retrieve an item from a node list
 *
 * \param list   The list to retrieve the item from
 * \param index  The list index to retrieve
 * \param node   Pointer to location to receive item
 * \return DOM_NO_ERR.
 *
 * ::index is a zero-based index into ::list.
 * ::index lies in the range [0, length-1]
 *
 * The returned node will have had its reference count increased. The client
 * should unref the node once it has finished with it.
 */
dom_exception _dom_nodelist_item(dom_nodelist *list,
		uint32_t index, dom_node **node)
{
	dom_node_internal *cur = list->root->first_child;
	uint32_t count = 0;

	/* Traverse data structure */
	while (cur != NULL) {
		/* Process current node */
		if (list->type == DOM_NODELIST_CHILDREN) {
			count++;
		} else if (list->type == DOM_NODELIST_BY_NAME) {
			if (list->data.n.any_name == true || (
					cur->name != NULL && 
					dom_string_isequal(cur->name, 
						list->data.n.name))) {
				if (cur->type == DOM_ELEMENT_NODE)
					count++;
			}
		} else if (list->type == DOM_NODELIST_BY_NAME_CASELESS) {
			if (list->data.n.any_name == true || (
					cur->name != NULL && 
					dom_string_caseless_isequal(cur->name, 
						list->data.n.name))) {
				if (cur->type == DOM_ELEMENT_NODE)
					count++;
			}
		} else if (list->type == DOM_NODELIST_BY_NAMESPACE) {
			if (list->data.ns.any_namespace == true || 
					(cur->namespace != NULL &&
					dom_string_isequal(cur->namespace,
						list->data.ns.namespace))) {
				if (list->data.ns.any_localname == true ||
						(cur->name != NULL &&
						dom_string_isequal(cur->name,
						list->data.ns.localname))) {
					if (cur->type == DOM_ELEMENT_NODE)
						count++;
				}
			}
		} else if (list->type == DOM_NODELIST_BY_NAMESPACE_CASELESS) {
			if (list->data.ns.any_namespace == true || 
					(cur->namespace != NULL &&
					dom_string_caseless_isequal(
						cur->namespace,
						list->data.ns.namespace))) {
				if (list->data.ns.any_localname == true ||
						(cur->name != NULL &&
						dom_string_caseless_isequal(
						cur->name,
						list->data.ns.localname))) {
					if (cur->type == DOM_ELEMENT_NODE)
						count++;
				}
			}
		} else {
			assert("Unknown list type" == NULL);
		}

		/* Stop if this is the requested index */
		if ((index + 1) == count) {
			break;
		}

		/* Now, find next node */
		if (list->type == DOM_NODELIST_CHILDREN) {
			/* Just interested in sibling list */
			cur = cur->next;
		} else {
			/* Want a full in-order tree traversal */
			if (cur->first_child != NULL) {
				/* Has children */
				cur = cur->first_child;
			} else if (cur->next != NULL) {
				/* No children, but has siblings */
				cur = cur->next;
			} else {
				/* No children or siblings.
				 * Find first unvisited relation. */
				dom_node_internal *parent = cur->parent;

				while (parent != list->root &&
						cur == parent->last_child) {
					cur = parent;
					parent = parent->parent;
				}

				cur = cur->next;
			}
		}
	}

	if (cur != NULL) {
		dom_node_ref(cur);
	}
	*node = (dom_node *) cur;

	return DOM_NO_ERR;
}

/**
 * Match a nodelist instance against a set of nodelist creation parameters
 *
 * \param list       List to match
 * \param type	     The type of the NodeList
 * \param root       Root node of subtree that list applies to
 * \param tagname    Name of nodes in list (or NULL)
 * \param namespace  Namespace part of nodes in list (or NULL)
 * \param localname  Local part of nodes in list (or NULL)
 * \return true if list matches, false otherwise
 */
bool _dom_nodelist_match(dom_nodelist *list, nodelist_type type,
		dom_node_internal *root, dom_string *tagname, 
		dom_string *namespace, dom_string *localname)
{
	if (list->root != root)
		return false;

	if (list->type != type)
		return false;
	
	switch (list->type) {
	case DOM_NODELIST_CHILDREN:
		return true;
	case DOM_NODELIST_BY_NAME:
		return dom_string_isequal(list->data.n.name, tagname);
	case DOM_NODELIST_BY_NAMESPACE:
		return dom_string_isequal(list->data.ns.namespace, namespace) &&
			dom_string_isequal(list->data.ns.localname, localname);
	case DOM_NODELIST_BY_NAME_CASELESS:
		return dom_string_caseless_isequal(list->data.n.name, tagname);
	case DOM_NODELIST_BY_NAMESPACE_CASELESS:
		return dom_string_caseless_isequal(list->data.ns.namespace,
						   namespace) &&
			dom_string_caseless_isequal(list->data.ns.localname,
						    localname);
	}
	
	return false;
}

/**
 * Test whether the two NodeList are equal
 *
 * \param l1  One list
 * \param l2  The other list
 * \reutrn true for equal, false otherwise.
 */
bool _dom_nodelist_equal(dom_nodelist *l1, dom_nodelist *l2)
{
	return _dom_nodelist_match(l1, l1->type, l2->root, l2->data.n.name, 
			l2->data.ns.namespace, l2->data.ns.localname);
}

