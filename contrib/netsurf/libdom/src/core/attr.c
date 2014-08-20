/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dom/core/attr.h>
#include <dom/core/document.h>
#include <dom/core/node.h>
#include <dom/core/string.h>

#include "core/attr.h"
#include "core/document.h"
#include "core/entity_ref.h"
#include "core/node.h"
#include "core/element.h"
#include "utils/utils.h"

struct dom_element;

/**
 * DOM attribute node
 */
struct dom_attr {
	struct dom_node_internal base;	/**< Base node */

	struct dom_type_info *schema_type_info;	/**< Type information */

	dom_attr_type type;	/**< The type of this attribute */
	
	union {
		uint32_t lvalue;
		unsigned short svalue;
		bool bvalue;
	} value;	/**< The special type value of this attribute */

	bool specified;	/**< Whether the attribute is specified or default */

	bool is_id;	/**< Whether this attribute is a ID attribute */

	bool read_only;	/**< Whether this attribute is readonly */
};

/* The vtable for dom_attr node */
static struct dom_attr_vtable attr_vtable = {
	{
		{
			DOM_NODE_EVENT_TARGET_VTABLE,
		},
		DOM_NODE_VTABLE_ATTR
	},
	DOM_ATTR_VTABLE
};

/* The protected vtable for dom_attr */
static struct dom_node_protect_vtable attr_protect_vtable = {
	DOM_ATTR_PROTECT_VTABLE
};


/* -------------------------------------------------------------------- */

/* Constructor and destructor */

/**
 * Create an attribute node
 *
 * \param doc        The owning document
 * \param name       The (local) name of the node to create
 * \param namespace  The namespace URI of the attribute, or NULL
 * \param prefix     The namespace prefix of the attribute, or NULL
 * \param specified  Whether this attribute is specified
 * \param result     Pointer to location to receive created attribute
 * \return DOM_NO_ERR     on success,
 *         DOM_NO_MEM_ERR on memory exhaustion.
 *
 * ::doc and ::name will have their reference counts increased. The 
 * caller should make sure that ::name is a valid NCName here.
 *
 * The returned attribute will already be referenced.
 */
dom_exception _dom_attr_create(struct dom_document *doc,
		dom_string *name, dom_string *namespace,
		dom_string *prefix, bool specified, 
		struct dom_attr **result)
{
	struct dom_attr *a;
	dom_exception err;

	/* Allocate the attribute node */
	a = malloc(sizeof(struct dom_attr));
	if (a == NULL)
		return DOM_NO_MEM_ERR;

	/* Initialise the vtable */
	a->base.base.vtable = &attr_vtable;
	a->base.vtable = &attr_protect_vtable;

	/* Initialise the class */
	err = _dom_attr_initialise(a, doc, name, namespace, prefix, specified, 
			result);
	if (err != DOM_NO_ERR) {
		free(a);
		return err;
	}

	return DOM_NO_ERR;
}

/**
 * Initialise a dom_attr
 *
 * \param a          The dom_attr
 * \param doc        The document
 * \param name       The name of this attribute node
 * \param namespace  The namespace of this attribute
 * \param prefix     The prefix
 * \param specified  Whether this node is a specified one
 * \param result     The returned node
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_attr_initialise(dom_attr *a, 
		struct dom_document *doc, dom_string *name,
		dom_string *namespace, dom_string *prefix,
		bool specified, struct dom_attr **result)
{
	dom_exception err;

	err = _dom_node_initialise(&a->base, doc, DOM_ATTRIBUTE_NODE,
			name, NULL, namespace, prefix);
	if (err != DOM_NO_ERR) {
		return err;
	}

	a->specified = specified;
	a->schema_type_info = NULL;
	a->is_id = false;
	/* The attribute type is unset when it is created */
	a->type = DOM_ATTR_UNSET;
	a->read_only = false;

	*result = a;

	return DOM_NO_ERR;
}

/**
 * The destructor of dom_attr
 *
 * \param attr  The attribute
 */
void _dom_attr_finalise(dom_attr *attr)
{
	/* Now, clean up this node and destroy it */

	if (attr->schema_type_info != NULL) {
		/** \todo destroy schema type info */
	}

	_dom_node_finalise(&attr->base);
}

/**
 * Destroy an attribute node
 *
 * \param attr  The attribute to destroy
 *
 * The contents of ::attr will be destroyed and ::attr will be freed
 */
void _dom_attr_destroy(struct dom_attr *attr)
{
	_dom_attr_finalise(attr);

	free(attr);
}

/*-----------------------------------------------------------------------*/
/* Following are our implementation specific APIs */

/**
 * Get the Attr Node type
 *
 * \param a  The attribute node
 * \return the type
 */
dom_attr_type dom_attr_get_type(dom_attr *a)
{
	return a->type;
}

/**
 * Get the integer value of this attribute
 *
 * \param a      The attribute object
 * \param value  The returned value
 * \return DOM_NO_ERR on success,
 *         DOM_ATTR_WRONG_TYPE_ERR if the attribute node is not a integer
 *                                 attribute
 */
dom_exception dom_attr_get_integer(dom_attr *a, uint32_t *value)
{
	if (a->type != DOM_ATTR_INTEGER)
		return DOM_ATTR_WRONG_TYPE_ERR;
	
	*value = a->value.lvalue;

	return DOM_NO_ERR;
}

/**
 * Set the integer value of this attribute
 *
 * \param a      The attribute object
 * \param value  The new value
 * \return DOM_NO_ERR on success,
 *         DOM_ATTR_WRONG_TYPE_ERR if the attribute node is not a integer
 *                                 attribute
 */
dom_exception dom_attr_set_integer(dom_attr *a, uint32_t value)
{
	struct dom_document *doc;
	struct dom_node_internal *ele;
	bool success = true;
	dom_exception err;

	/* If this is the first set method, we should fix this attribute
	 * type */
	if (a->type == DOM_ATTR_UNSET)
		a->type = DOM_ATTR_INTEGER;

	if (a->type != DOM_ATTR_INTEGER)
		return DOM_ATTR_WRONG_TYPE_ERR;
	
	if (a->value.lvalue == value)
		return DOM_NO_ERR;
	
	a->value.lvalue = value;

	doc = dom_node_get_owner(a);
	ele = dom_node_get_parent(a);
	err = _dom_dispatch_attr_modified_event(doc, ele, NULL, NULL,
			(dom_event_target *) a, NULL,
			DOM_MUTATION_MODIFICATION, &success);
	if (err != DOM_NO_ERR)
		return err;
	
	success = true;
	err = _dom_dispatch_subtree_modified_event(doc,
			(dom_event_target *) a, &success);
	return err;
}

/**
 * Get the short value of this attribute
 *
 * \param a      The attribute object
 * \param value  The returned value
 * \return DOM_NO_ERR on success,
 *         DOM_ATTR_WRONG_TYPE_ERR if the attribute node is not a short
 *                                 attribute
 */
dom_exception dom_attr_get_short(dom_attr *a, unsigned short *value)
{
	if (a->type != DOM_ATTR_SHORT)
		return DOM_ATTR_WRONG_TYPE_ERR;
	
	*value = a->value.svalue;

	return DOM_NO_ERR;
}

/**
 * Set the short value of this attribute
 *
 * \param a      The attribute object
 * \param value  The new value
 * \return DOM_NO_ERR on success,
 *         DOM_ATTR_WRONG_TYPE_ERR if the attribute node is not a short
 *                                 attribute
 */
dom_exception dom_attr_set_short(dom_attr *a, unsigned short value)
{
	struct dom_document *doc;
	struct dom_node_internal *ele;
	bool success = true;
	dom_exception err;

	/* If this is the first set method, we should fix this attribute
	 * type */
	if (a->type == DOM_ATTR_UNSET)
		a->type = DOM_ATTR_SHORT;

	if (a->type != DOM_ATTR_SHORT)
		return DOM_ATTR_WRONG_TYPE_ERR;
	
	if (a->value.svalue == value)
		return DOM_NO_ERR;
	
	a->value.svalue = value;

	doc = dom_node_get_owner(a);
	ele = dom_node_get_parent(a);
	err = _dom_dispatch_attr_modified_event(doc, ele, NULL, NULL,
			(dom_event_target *) a, NULL,
			DOM_MUTATION_MODIFICATION, &success);
	if (err != DOM_NO_ERR)
		return err;
	
	success = true;
	err = _dom_dispatch_subtree_modified_event(doc,
			(dom_event_target *) a, &success);
	return err;
}

/**
 * Get the bool value of this attribute
 *
 * \param a      The attribute object
 * \param value  The returned value
 * \return DOM_NO_ERR on success,
 *         DOM_ATTR_WRONG_TYPE_ERR if the attribute node is not a bool
 *                                 attribute
 */
dom_exception dom_attr_get_bool(dom_attr *a, bool *value)
{
	if (a->type != DOM_ATTR_BOOL)
		return DOM_ATTR_WRONG_TYPE_ERR;
	
	*value = a->value.bvalue;

	return DOM_NO_ERR;
}

/**
 * Set the bool value of this attribute
 *
 * \param a      The attribute object
 * \param value  The new value
 * \return DOM_NO_ERR on success,
 *         DOM_ATTR_WRONG_TYPE_ERR if the attribute node is not a bool
 *                                 attribute
 */
dom_exception dom_attr_set_bool(dom_attr *a, bool value)
{
	struct dom_document *doc;
	struct dom_node_internal *ele;
	bool success = true;
	dom_exception err;

	/* If this is the first set method, we should fix this attribute
	 * type */
	if (a->type == DOM_ATTR_UNSET)
		a->type = DOM_ATTR_BOOL;

	if (a->type != DOM_ATTR_BOOL)
		return DOM_ATTR_WRONG_TYPE_ERR;
	
	if (a->value.bvalue == value)
		return DOM_NO_ERR;
	
	a->value.bvalue = value;

	doc = dom_node_get_owner(a);
	ele = dom_node_get_parent(a);
	err = _dom_dispatch_attr_modified_event(doc, ele, NULL, NULL,
			(dom_event_target *) a, NULL,
			DOM_MUTATION_MODIFICATION, &success);
	if (err != DOM_NO_ERR)
		return err;
	
	success = true;
	err = _dom_dispatch_subtree_modified_event(doc,
			(dom_event_target *) a, &success);
	return err;
}

/**
 * Set the node as a readonly attribute
 *
 * \param a  The attribute
 */
void dom_attr_mark_readonly(dom_attr *a)
{
	a->read_only = true;
}

/* -------------------------------------------------------------------- */

/* The public virtual functions */

/**
 * Retrieve an attribute's name
 *
 * \param attr    Attribute to retrieve name from
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception _dom_attr_get_name(struct dom_attr *attr,
		dom_string **result)
{
	/* This is the same as nodeName */
	return dom_node_get_node_name(attr, result);
}

/**
 * Determine if attribute was specified or default
 *
 * \param attr    Attribute to inspect
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_attr_get_specified(struct dom_attr *attr, bool *result)
{
	*result = attr->specified;

	return DOM_NO_ERR;
}

/**
 * Retrieve an attribute's value
 *
 * \param attr    Attribute to retrieve value from
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception _dom_attr_get_value(struct dom_attr *attr,
		dom_string **result)
{
	struct dom_node_internal *a = (struct dom_node_internal *) attr;
	struct dom_node_internal *c;
	dom_string *value, *temp;
	dom_exception err;
        
	/* Attempt to shortcut for a single text node child with value */
	if ((a->first_child != NULL) && 
	    (a->first_child == a->last_child) &&
	    (a->first_child->type == DOM_TEXT_NODE) &&
	    (a->first_child->value != NULL)) {
		*result = dom_string_ref(a->first_child->value);
		return DOM_NO_ERR;
	}
	
	err = dom_string_create(NULL, 0, &value);
	if (err != DOM_NO_ERR) {
		return err;
	}

	/* Force unknown types to strings, if necessary */
	if (attr->type == DOM_ATTR_UNSET && a->first_child != NULL) {
		attr->type = DOM_ATTR_STRING;
	}

	/* If this attribute node is not a string one, we just return an empty
	 * string */
	if (attr->type != DOM_ATTR_STRING) {
		*result = value;
		return DOM_NO_ERR;
	}

	/* Traverse children, building a string representation as we go */
	for (c = a->first_child; c != NULL; c = c->next) {
		if (c->type == DOM_TEXT_NODE && c->value != NULL) {
			/* Append to existing value */
			err = dom_string_concat(value, c->value, &temp);
			if (err != DOM_NO_ERR) {
				dom_string_unref(value);
				return err;
			}

			/* Finished with previous value */
			dom_string_unref(value);

			/* Claim new value */
			value = temp;
		} else if (c->type == DOM_ENTITY_REFERENCE_NODE) {
			dom_string *tr;

			/* Get textual representation of entity */
			err = _dom_entity_reference_get_textual_representation(
					(struct dom_entity_reference *) c,
					&tr);
			if (err != DOM_NO_ERR) {
				dom_string_unref(value);
				return err;
			}

			/* Append to existing value */
			err = dom_string_concat(value, tr, &temp);
			if (err != DOM_NO_ERR) {
				dom_string_unref(tr);
				dom_string_unref(value);
				return err;
			}

			/* No int32_ter need textual representation */
			dom_string_unref(tr);

			/* Finished with previous value */
			dom_string_unref(value);

			/* Claim new value */
			value = temp;
		}
	}

	*result = value;

	return DOM_NO_ERR;
}

/**
 * Set an attribute's value
 *
 * \param attr   Attribute to retrieve value from
 * \param value  New value for attribute
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if attribute is readonly.
 */
dom_exception _dom_attr_set_value(struct dom_attr *attr,
		dom_string *value)
{
	struct dom_node_internal *a = (struct dom_node_internal *) attr;
	struct dom_node_internal *c, *d;
	struct dom_text *text;
	dom_exception err;
	dom_string *name = NULL;
	dom_string *parsed = NULL;

	/* Ensure attribute is writable */
	if (_dom_node_readonly(a))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	/* If this is the first set method, we should fix this attribute
	 * type */
	if (attr->type == DOM_ATTR_UNSET)
		attr->type = DOM_ATTR_STRING;
	
	if (attr->type != DOM_ATTR_STRING)
		return DOM_ATTR_WRONG_TYPE_ERR;
	
	err = _dom_attr_get_name(attr, &name);
	if (err != DOM_NO_ERR)
		return err;
	
	err = dom_element_parse_attribute(a->parent, name, value, &parsed);
	dom_string_unref(name);
	if (err != DOM_NO_ERR) {
		return err;
	}

	/* Create text node containing new value */
	err = dom_document_create_text_node(a->owner, parsed, &text);
	dom_string_unref(parsed);
	if (err != DOM_NO_ERR)
		return err;
	
	/* Destroy children of this node */
	for (c = a->first_child; c != NULL; c = d) {
		d = c->next;

		/* Detach child */
		c->parent = NULL;

		/* Detach from sibling list */
		c->previous = NULL;
		c->next = NULL;

		dom_node_try_destroy(c);
	}

	/* And insert the text node as the value */
	((struct dom_node_internal *) text)->parent = a;
	a->first_child = a->last_child = (struct dom_node_internal *) text;
	dom_node_unref(text);
	dom_node_remove_pending(text);

	/* Now the attribute node is specified */
	attr->specified = true;

	return DOM_NO_ERR;
}

/**
 * Retrieve the owning element of an attribute
 *
 * \param attr    The attribute to extract owning element from
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * The returned node will have its reference count increased. The caller
 * should unref it once it has finished with it.
 */
dom_exception _dom_attr_get_owner(struct dom_attr *attr,
		struct dom_element **result)
{
	struct dom_node_internal *a = (struct dom_node_internal *) attr;

	/* If there is an owning element, increase its reference count */
	if (a->parent != NULL)
		dom_node_ref(a->parent);

	*result = (struct dom_element *) a->parent;

	return DOM_NO_ERR;
}

/**
 * Retrieve an attribute's type information
 *
 * \param attr    The attribute to extract type information from
 * \param result  Pointer to location to receive result
 * \return DOM_NOT_SUPPORTED_ERR, we don't support this API now.
 *
 * The returned type info will have its reference count increased. The caller
 * should unref it once it has finished with it.
 */
dom_exception _dom_attr_get_schema_type_info(struct dom_attr *attr,
		struct dom_type_info **result)
{
	UNUSED(attr);
	UNUSED(result);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Determine if an attribute if of type ID
 *
 * \param attr    The attribute to inspect
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_attr_is_id(struct dom_attr *attr, bool *result)
{
	*result = attr->is_id;

	return DOM_NO_ERR;
}

/*------------- The overload virtual functions ------------------------*/

/* Overload function of Node, please refer node.c for the detail of this 
 * function. */
dom_exception _dom_attr_get_node_value(dom_node_internal *node,
		dom_string **result)
{
	dom_attr *attr = (dom_attr *) node;

	return _dom_attr_get_value(attr, result);
}

/* Overload function of Node, please refer node.c for the detail of this 
 * function. */
dom_exception _dom_attr_clone_node(dom_node_internal *node, bool deep,
		dom_node_internal **result)
{
	dom_exception err;
	dom_attr *attr;

	/* Discard the warnings */
	UNUSED(deep);

	/* Clone an Attr always clone all its children */
	err = _dom_node_clone_node(node, true, result);
	if (err != DOM_NO_ERR)
		return err;
	
	attr = (dom_attr *) *result;
	/* Clone an Attr always result a specified Attr, 
	 * see DOM Level 3 Node.cloneNode */
	attr->specified = true;

	return DOM_NO_ERR;
}

/* Overload function of Node, please refer node.c for the detail of this 
 * function. */
dom_exception _dom_attr_set_prefix(dom_node_internal *node,
		dom_string *prefix)
{
	/* Really I don't know whether there should something
	 * special to do here */
	return _dom_node_set_prefix(node, prefix);
}

/* Overload function of Node, please refer node.c for the detail of this 
 * function. */
dom_exception _dom_attr_lookup_prefix(dom_node_internal *node,
		dom_string *namespace, dom_string **result)
{
	struct dom_element *owner;
	dom_exception err;

	err = dom_attr_get_owner_element(node, &owner);
	if (err != DOM_NO_ERR)
		return err;
	
	if (owner == NULL) {
		*result = NULL;
		return DOM_NO_ERR;
	}

	return dom_node_lookup_prefix(owner, namespace, result);
}

/* Overload function of Node, please refer node.c for the detail of this 
 * function. */
dom_exception _dom_attr_is_default_namespace(dom_node_internal *node,
		dom_string *namespace, bool *result)
{
	struct dom_element *owner;
	dom_exception err;

	err = dom_attr_get_owner_element(node, &owner);
	if (err != DOM_NO_ERR)
		return err;
	
	if (owner == NULL) {
		*result = false;
		return DOM_NO_ERR;
	}

	return dom_node_is_default_namespace(owner, namespace, result);
}

/* Overload function of Node, please refer node.c for the detail of this 
 * function. */
dom_exception _dom_attr_lookup_namespace(dom_node_internal *node,
		dom_string *prefix, dom_string **result)
{
	struct dom_element *owner;
	dom_exception err;

	err = dom_attr_get_owner_element(node, &owner);
	if (err != DOM_NO_ERR)
		return err;
	
	if (owner == NULL) {
		*result = NULL;
		return DOM_NO_ERR;
	}

	return dom_node_lookup_namespace(owner, prefix, result);
}


/*----------------------------------------------------------------------*/

/* The protected virtual functions */

/* The virtual destroy function of this class */
void __dom_attr_destroy(dom_node_internal *node)
{
	_dom_attr_destroy((dom_attr *) node);
}

/* The memory allocator of this class */
dom_exception _dom_attr_copy(dom_node_internal *n, dom_node_internal **copy)
{
	dom_attr *old = (dom_attr *) n;
	dom_attr *a;
	dom_exception err;
	
	a = malloc(sizeof(struct dom_attr));
	if (a == NULL)
		return DOM_NO_MEM_ERR;

	err = dom_node_copy_internal(n, a);
	if (err != DOM_NO_ERR) {
		free(a);
		return err;
	}
	
	a->specified = old->specified;

	/* TODO: deal with dom_type_info, it get no definition ! */
	a->schema_type_info = NULL;

	a->is_id = old->is_id;

	a->type = old->type;

	a->value = old->value;

	/* TODO: is this correct? */
	a->read_only = false;

	*copy = (dom_node_internal *) a;

	return DOM_NO_ERR;
}


/**
 * Set/Unset whether this attribute is a ID attribute 
 *
 * \param attr   The attribute
 * \param is_id  Whether it is a ID attribute
 */
void _dom_attr_set_isid(struct dom_attr *attr, bool is_id)
{
	attr->is_id = is_id;
}

/**
 * Set/Unset whether the attribute is a specified one.
 *
 * \param attr       The attribute node
 * \param specified  Whether this attribute is a specified one
 */
void _dom_attr_set_specified(struct dom_attr *attr, bool specified)
{
	attr->specified = specified;
}

/**
 * Whether this attribute node is readonly
 *
 * \param a  The node
 * \return true if this Attr is readonly, false otherwise
 */
bool _dom_attr_readonly(const dom_attr *a)
{
	return a->read_only;
}

