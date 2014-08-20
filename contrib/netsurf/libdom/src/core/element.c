/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libwapcaplet/libwapcaplet.h>

#include <dom/dom.h>
#include <dom/core/attr.h>
#include <dom/core/element.h>
#include <dom/core/node.h>
#include <dom/core/string.h>
#include <dom/core/document.h>
#include <dom/events/events.h>

#include "core/attr.h"
#include "core/document.h"
#include "core/element.h"
#include "core/node.h"
#include "core/namednodemap.h"
#include "utils/validate.h"
#include "utils/namespace.h"
#include "utils/utils.h"
#include "utils/list.h"
#include "events/mutation_event.h"

struct dom_element_vtable _dom_element_vtable = {
	{
		{
			DOM_NODE_EVENT_TARGET_VTABLE
		},
		DOM_NODE_VTABLE_ELEMENT
	},
	DOM_ELEMENT_VTABLE
};

static struct dom_element_protected_vtable element_protect_vtable = {
	{
		DOM_NODE_PROTECT_VTABLE_ELEMENT
	},
	DOM_ELEMENT_PROTECT_VTABLE
};


typedef struct dom_attr_list {
	struct list_entry list; /**< Linked list links to prev/next entries */
	
	struct dom_attr *attr;

	struct dom_string *name;
	struct dom_string *namespace;
} dom_attr_list;


/**
 * Destroy element's class cache
 *
 * \param ele  The element
 */
static void _dom_element_destroy_classes(struct dom_element *ele)
{
	/* Destroy the pre-separated class names */
	if (ele->classes != NULL) {
		unsigned int class;
		for (class = 0; class < ele->n_classes; class++) {
			lwc_string_unref(ele->classes[class]);
		}
		free(ele->classes);
	}

	ele->n_classes = 0;
	ele->classes = NULL;
}


/**
 * Create element's class cache from class attribute value
 *
 * \param ele    The element
 * \param value  The class attribute value
 *
 * Destroys any existing cached classes.
 */
static dom_exception _dom_element_create_classes(struct dom_element *ele,
		const char *value)
{
	const char *pos;
	lwc_string **classes = NULL;
	uint32_t n_classes = 0;

	/* Any existing cached classes are replaced; destroy them */
	_dom_element_destroy_classes(ele);

	/* Count number of classes */
	for (pos = value; *pos != '\0'; ) {
		if (*pos != ' ') {
			while (*pos != ' ' && *pos != '\0')
				pos++;
			n_classes++;
		} else {
			while (*pos == ' ')
				pos++;
		}
	}

	/* If there are some, unpack them */
	if (n_classes > 0) {
		classes = malloc(n_classes * sizeof(lwc_string *));
		if (classes == NULL)
			return DOM_NO_MEM_ERR;

		for (pos = value, n_classes = 0;
				*pos != '\0'; ) {
			if (*pos != ' ') {
				const char *s = pos;
				while (*pos != ' ' && *pos != '\0')
					pos++;
				if (lwc_intern_string(s, pos - s, 
						&classes[n_classes++]) !=
						lwc_error_ok)
					goto error;
			} else {
				while (*pos == ' ')
					pos++;
			}
		}
	}

	ele->n_classes = n_classes;
	ele->classes = classes;

	return DOM_NO_ERR;
error:
	while (n_classes > 0)
		lwc_string_unref(classes[--n_classes]);

	free(classes);
		
	return DOM_NO_MEM_ERR;
}

/* Attribute linked list releated functions */

/**
 * Get the next attribute in the list
 *
 * \param n  The attribute list node
 * \return The next attribute node
 */
static dom_attr_list * _dom_element_attr_list_next(const dom_attr_list *n)
{
	return (dom_attr_list *)(n->list.next);
}

/**
 * Unlink an attribute list node from its linked list
 *
 * \param n  The attribute list node
 */
static void _dom_element_attr_list_node_unlink(dom_attr_list *n)
{
	if (n == NULL)
		return;

	list_del(&n->list);
}

/**
 * Insert attribute list node into attribute list
 *
 * \param list      The list header
 * \param new_attr  The attribute node to insert
 */
static void _dom_element_attr_list_insert(dom_attr_list *list,
		dom_attr_list *new_attr)
{
	assert(list != NULL);
	assert(new_attr != NULL);
	list_append(&list->list, &new_attr->list);
}

/**
 * Get attribute from attribute list, which matches given name
 *
 * \param list  The attribute list to search
 * \param name  The name of the attribute to search for
 * \param name  The namespace of the attribute to search for (may be NULL)
 * \return the matching attribute, or NULL if none found
 */
static dom_attr_list * _dom_element_attr_list_find_by_name(
		dom_attr_list *list, dom_string *name, dom_string *namespace)
{
	dom_attr_list *attr = list;

	if (list == NULL || name == NULL)
		return NULL;

	do {
		if (((namespace == NULL && attr->namespace == NULL) ||
				(namespace != NULL && attr->namespace != NULL &&
						dom_string_isequal(namespace,
						attr->namespace))) &&
				dom_string_isequal(name, attr->name)) {
			/* Both have NULL namespace or matching namespace,
			 * and both have same name */
			return attr;
		}

		attr = _dom_element_attr_list_next(attr);
		assert(attr != NULL);
	} while (attr != list);

	return NULL;
}

/**
 * Get the number of elements in this attribute list
 *
 * \param list  The attribute list
 * \return the number attribute list node elements
 */
static unsigned int _dom_element_attr_list_length(dom_attr_list *list)
{
	dom_attr_list *attr = list;
	unsigned int count = 0;

	if (list == NULL)
		return count;

	do {
		count++;

		attr = _dom_element_attr_list_next(attr);
	} while (attr != list);

	return count;
}

/**
 * Get the attribute list node at the given index
 *
 * \param list   The attribute list
 * \param index  The index number
 * \return the attribute list node at given index
 */
static dom_attr_list * _dom_element_attr_list_get_by_index(dom_attr_list *list,
		unsigned int index)
{
	dom_attr_list *attr = list;

	if (list == NULL)
		return NULL;

	do {
		if (--index == 0)
			return attr;

		attr = _dom_element_attr_list_next(attr);
	} while (attr != list);

	return NULL;
}

/**
 * Destroy an attribute list node, and its attribute
 *
 * \param n  The attribute list node to destroy
 */
static void _dom_element_attr_list_node_destroy(dom_attr_list *n)
{
	dom_node_internal *a;
	dom_document *doc;

	assert(n != NULL);
	assert(n->attr != NULL);
	assert(n->name != NULL);

	a = (dom_node_internal *) n->attr;

	/* Need to destroy classes cache, when removing class attribute */
	doc = a->owner;
	if (n->namespace == NULL &&
			dom_string_isequal(n->name, doc->class_string)) {
		_dom_element_destroy_classes((dom_element *)(a->parent));
	}

	/* Destroy rest of list node */
	dom_string_unref(n->name);

	if (n->namespace != NULL)
		dom_string_unref(n->namespace);

	a->parent = NULL;
	dom_node_try_destroy(a);

	free(n);
}

/**
 * Create an attribute list node
 *
 * \param attr       The attribute to create a list node for
 * \param name       The attribute name
 * \param namespace  The attribute namespace (may be NULL)
 * \return the new attribute list node, or NULL on failure
 */
static dom_attr_list * _dom_element_attr_list_node_create(dom_attr *attr,
		dom_element *ele, dom_string *name, dom_string *namespace)
{
	dom_attr_list *new_list_node;
	dom_node_internal *a;
	dom_document *doc;

	if (attr == NULL || name == NULL)
		return NULL;

	new_list_node = malloc(sizeof(*new_list_node));
	if (new_list_node == NULL)
		return NULL;

	list_init(&new_list_node->list);

	new_list_node->attr = attr;
	new_list_node->name = name;
	new_list_node->namespace = namespace;

	a = (dom_node_internal *) attr;
	doc = a->owner;
	if (namespace == NULL &&
			dom_string_isequal(name, doc->class_string)) {
		dom_string *value;

		if (DOM_NO_ERR != _dom_attr_get_value(attr, &value)) {
			_dom_element_attr_list_node_destroy(new_list_node);
			return NULL;
		}

		if (DOM_NO_ERR != _dom_element_create_classes(ele,
				dom_string_data(value))) {
			_dom_element_attr_list_node_destroy(new_list_node);
			dom_string_unref(value);
			return NULL;
		}

		dom_string_unref(value);
	}

	return new_list_node;
}

/**
 * Destroy an entire attribute list, and its attributes
 *
 * \param list  The attribute list to destroy
 */
static void _dom_element_attr_list_destroy(dom_attr_list *list)
{
	dom_attr_list *attr = list;
	dom_attr_list *next = list;

	if (list == NULL)
		return;

	do {
		attr = next;
		next = _dom_element_attr_list_next(attr);

		_dom_element_attr_list_node_unlink(attr);
		_dom_element_attr_list_node_destroy(attr);
	} while (next != attr);

	return;
}

/**
 * Clone an attribute list node, and its attribute
 *
 * \param n     The attribute list node to clone
 * \param newe  Element to clone attribute for
 * \return the new attribute list node, or NULL on failure
 */
static dom_attr_list *_dom_element_attr_list_node_clone(dom_attr_list *n,
		dom_element *newe)
{
	dom_attr *clone = NULL;
	dom_attr_list *new_list_node;
	dom_exception err;

	assert(n != NULL);
	assert(n->attr != NULL);
	assert(n->name != NULL);

	new_list_node = malloc(sizeof(*new_list_node));
	if (new_list_node == NULL)
		return NULL;

	list_init(&new_list_node->list);

	new_list_node->name = NULL;
	new_list_node->namespace = NULL;

	err = dom_node_clone_node(n->attr, true, (void *) &clone);
	if (err != DOM_NO_ERR) {
		free(new_list_node);
		return NULL;
	}

	dom_node_set_parent(clone, newe);
	dom_node_remove_pending(clone);
	dom_node_unref(clone);
	new_list_node->attr = clone;

	if (n->name != NULL)
		new_list_node->name = dom_string_ref(n->name);

	if (n->namespace != NULL)
		new_list_node->namespace = dom_string_ref(n->namespace);

	return new_list_node;
}

/**
 * Clone an entire attribute list, and its attributes
 *
 * \param list  The attribute list to clone
 * \param newe  Element to clone list for
 * \return the new attribute list, or NULL on failure
 */
static dom_attr_list *_dom_element_attr_list_clone(dom_attr_list *list,
		dom_element *newe)
{
	dom_attr_list *attr = list;

	dom_attr_list *new_list = NULL;
	dom_attr_list *new_list_node = NULL;

	if (list == NULL)
		return NULL;

	do {
		new_list_node = _dom_element_attr_list_node_clone(attr, newe);
		if (new_list_node == NULL) {
			if (new_list != NULL)
				_dom_element_attr_list_destroy(new_list);
			return NULL;
		}

		if (new_list == NULL) {
			new_list = new_list_node;
		} else {
			_dom_element_attr_list_insert(new_list, new_list_node);
		}

		attr = _dom_element_attr_list_next(attr);
	} while (attr != list);

	return new_list;
}

static dom_exception _dom_element_get_attr(struct dom_element *element,
		dom_string *namespace, dom_string *name, dom_string **value);
static dom_exception _dom_element_set_attr(struct dom_element *element,
		dom_string *namespace, dom_string *name, dom_string *value);
static dom_exception _dom_element_remove_attr(struct dom_element *element,
		dom_string *namespace, dom_string *name);

static dom_exception _dom_element_get_attr_node(struct dom_element *element,
		dom_string *namespace, dom_string *name,
		struct dom_attr **result);
static dom_exception _dom_element_set_attr_node(struct dom_element *element,
		dom_string *namespace, struct dom_attr *attr,
		struct dom_attr **result);
static dom_exception _dom_element_remove_attr_node(struct dom_element *element,
		dom_string *namespace, struct dom_attr *attr,
		struct dom_attr **result);

static dom_exception _dom_element_has_attr(struct dom_element *element,
		dom_string *namespace, dom_string *name, bool *result);
static dom_exception _dom_element_set_id_attr(struct dom_element *element,
		dom_string *namespace, dom_string *name, bool is_id);


/* The operation set for namednodemap */
static dom_exception attributes_get_length(void *priv,
		uint32_t *length);
static dom_exception attributes_get_named_item(void *priv,
		dom_string *name, struct dom_node **node);
static dom_exception attributes_set_named_item(void *priv,
		struct dom_node *arg, struct dom_node **node);
static dom_exception attributes_remove_named_item(
		void *priv, dom_string *name,
		struct dom_node **node);
static dom_exception attributes_item(void *priv,
		uint32_t index, struct dom_node **node);
static dom_exception attributes_get_named_item_ns(
		void *priv, dom_string *namespace,
		dom_string *localname, struct dom_node **node);
static dom_exception attributes_set_named_item_ns(
		void *priv, struct dom_node *arg,
		struct dom_node **node);
static dom_exception attributes_remove_named_item_ns(
		void *priv, dom_string *namespace,
		dom_string *localname, struct dom_node **node);
static void attributes_destroy(void *priv);
static bool attributes_equal(void *p1, void *p2);

static struct nnm_operation attributes_opt = {
	attributes_get_length,
	attributes_get_named_item,
	attributes_set_named_item,
	attributes_remove_named_item,
	attributes_item,
	attributes_get_named_item_ns,
	attributes_set_named_item_ns,
	attributes_remove_named_item_ns,
	attributes_destroy,
	attributes_equal
};

/*----------------------------------------------------------------------*/
/* Constructors and Destructors */

/**
 * Create an element node
 *
 * \param doc        The owning document
 * \param name       The (local) name of the node to create
 * \param namespace  The namespace URI of the element, or NULL
 * \param prefix     The namespace prefix of the element, or NULL
 * \param result     Pointer to location to receive created element
 * \return DOM_NO_ERR                on success,
 *         DOM_INVALID_CHARACTER_ERR if ::name is invalid,
 *         DOM_NO_MEM_ERR            on memory exhaustion.
 *
 * ::doc, ::name, ::namespace and ::prefix will have their 
 * reference counts increased.
 *
 * The returned element will already be referenced.
 */
dom_exception _dom_element_create(struct dom_document *doc,
		dom_string *name, dom_string *namespace,
		dom_string *prefix, struct dom_element **result)
{
	/* Allocate the element */
	*result = malloc(sizeof(struct dom_element));
	if (*result == NULL)
		return DOM_NO_MEM_ERR;

	/* Initialise the vtables */
	(*result)->base.base.vtable = &_dom_element_vtable;
	(*result)->base.vtable = &element_protect_vtable;

	return _dom_element_initialise(doc, *result, name, namespace, prefix);
}

/**
 * Initialise an element node
 *
 * \param doc        The owning document
 * \param el	     The element
 * \param name       The (local) name of the node to create
 * \param namespace  The namespace URI of the element, or NULL
 * \param prefix     The namespace prefix of the element, or NULL
 * \return DOM_NO_ERR                on success,
 *         DOM_INVALID_CHARACTER_ERR if ::name is invalid,
 *         DOM_NO_MEM_ERR            on memory exhaustion.
 *
 * The caller should make sure that ::name is a valid NCName.
 *
 * ::doc, ::name, ::namespace and ::prefix will have their 
 * reference counts increased.
 *
 * The returned element will already be referenced.
 */
dom_exception _dom_element_initialise(struct dom_document *doc,
		struct dom_element *el, dom_string *name, 
		dom_string *namespace, dom_string *prefix)
{
	dom_exception err;

	assert(doc != NULL);

	el->attributes = NULL;

	/* Initialise the base class */
	err = _dom_node_initialise(&el->base, doc, DOM_ELEMENT_NODE,
			name, NULL, namespace, prefix);
	if (err != DOM_NO_ERR) {
		free(el);
		return err;
	}

	/* Perform our type-specific initialisation */
	el->id_ns = NULL;
	el->id_name = NULL;
	el->schema_type_info = NULL;

	el->n_classes = 0;
	el->classes = NULL;

	return DOM_NO_ERR;
}

/**
 * Finalise a dom_element
 *
 * \param ele  The element
 */
void _dom_element_finalise(struct dom_element *ele)
{
	/* Destroy attributes attached to this node */
	if (ele->attributes != NULL) {
		_dom_element_attr_list_destroy(ele->attributes);
		ele->attributes = NULL;
	}

	if (ele->schema_type_info != NULL) {
		/** \todo destroy schema type info */
	}

	/* Destroy the pre-separated class names */
	_dom_element_destroy_classes(ele);

	/* Finalise base class */
	_dom_node_finalise(&ele->base);
}

/**
 * Destroy an element
 *
 * \param element  The element to destroy
 *
 * The contents of ::element will be destroyed and ::element will be freed.
 */
void _dom_element_destroy(struct dom_element *element)
{
	_dom_element_finalise(element);

	/* Free the element */
	free(element);
}

/*----------------------------------------------------------------------*/

/* The public virtual functions */

/**
 * Retrieve an element's tag name
 *
 * \param element  The element to retrieve the name from
 * \param name     Pointer to location to receive name
 * \return DOM_NO_ERR      on success,
 *         DOM_NO_MEM_ERR  on memory exhaustion.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception _dom_element_get_tag_name(struct dom_element *element,
		dom_string **name)
{
	/* This is the same as nodeName */
	return dom_node_get_node_name((struct dom_node *) element, name);
}

/**
 * Retrieve an attribute from an element by name
 *
 * \param element  The element to retrieve attribute from
 * \param name     The attribute's name
 * \param value    Pointer to location to receive attribute's value
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception _dom_element_get_attribute(struct dom_element *element,
		dom_string *name, dom_string **value)
{
	return _dom_element_get_attr(element, NULL, name, value);
}

/**
 * Set an attribute on an element by name
 *
 * \param element  The element to set attribute on
 * \param name     The attribute's name
 * \param value    The attribute's value
 * \return DOM_NO_ERR                      on success,
 *         DOM_INVALID_CHARACTER_ERR       if ::name is invalid,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly.
 */
dom_exception _dom_element_set_attribute(struct dom_element *element,
		dom_string *name, dom_string *value)
{
	return _dom_element_set_attr(element, NULL, name, value);
}

/**
 * Remove an attribute from an element by name
 *
 * \param element  The element to remove attribute from
 * \param name     The name of the attribute to remove
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly.
 */
dom_exception _dom_element_remove_attribute(struct dom_element *element,
		dom_string *name)
{
	return _dom_element_remove_attr(element, NULL, name);
}

/**
 * Retrieve an attribute node from an element by name
 *
 * \param element  The element to retrieve attribute node from
 * \param name     The attribute's name
 * \param result   Pointer to location to receive attribute node
 * \return DOM_NO_ERR.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_element_get_attribute_node(struct dom_element *element, 
		dom_string *name, struct dom_attr **result)
{
	return _dom_element_get_attr_node(element, NULL, name, result);
}

/**
 * Set an attribute node on an element, replacing existing node, if present
 *
 * \param element  The element to add a node to
 * \param attr     The attribute node to add
 * \param result   Pointer to location to receive previous node
 * \return DOM_NO_ERR                      on success,
 *         DOM_WRONG_DOCUMENT_ERR          if ::attr does not belong to the
 *                                         same document as ::element,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_INUSE_ATTRIBUTE_ERR         if ::attr is already an attribute
 *                                         of another Element node.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_element_set_attribute_node(struct dom_element *element,
		struct dom_attr *attr, struct dom_attr **result)
{
	return _dom_element_set_attr_node(element, NULL, attr, result);
}

/**
 * Remove an attribute node from an element
 *
 * \param element  The element to remove attribute node from
 * \param attr     The attribute node to remove
 * \param result   Pointer to location to receive attribute node
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NOT_FOUND_ERR               if ::attr is not an attribute of
 *                                         ::element.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_element_remove_attribute_node(struct dom_element *element,
		struct dom_attr *attr, struct dom_attr **result)
{
	return _dom_element_remove_attr_node(element, NULL, attr, result);
}

/**
 * Retrieve a list of descendant elements of an element which match a given
 * tag name
 *
 * \param element  The root of the subtree to search
 * \param name     The tag name to match (or "*" for all tags)
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * The returned nodelist will have its reference count increased. It is
 * the responsibility of the caller to unref the nodelist once it has
 * finished with it.
 */
dom_exception _dom_element_get_elements_by_tag_name(
		struct dom_element *element, dom_string *name,
		struct dom_nodelist **result)
{
	dom_exception err;
	dom_node_internal *base = (dom_node_internal *) element;
	
	assert(base->owner != NULL);

	err = _dom_document_get_nodelist(base->owner, DOM_NODELIST_BY_NAME,
			(struct dom_node_internal *) element, name, NULL, 
			NULL, result);

	return err;
}

/**
 * Retrieve an attribute from an element by namespace/localname
 *
 * \param element    The element to retrieve attribute from
 * \param namespace  The attribute's namespace URI, or NULL
 * \param localname  The attribute's local name
 * \param value      Pointer to location to receive attribute's value
 * \return DOM_NO_ERR            on success,
 *         DOM_NOT_SUPPORTED_ERR if the implementation does not support
 *                               the feature "XML" and the language exposed
 *                               through the Document does not support
 *                               Namespaces.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception _dom_element_get_attribute_ns(struct dom_element *element,
		dom_string *namespace, dom_string *localname,
		dom_string **value)
{
	return _dom_element_get_attr(element, namespace, localname, value);
}

/**
 * Set an attribute on an element by namespace/qualified name
 *
 * \param element    The element to set attribute on
 * \param namespace  The attribute's namespace URI
 * \param qname      The attribute's qualified name
 * \param value      The attribute's value
 * \return DOM_NO_ERR                      on success,
 *         DOM_INVALID_CHARACTER_ERR       if ::qname is invalid,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NAMESPACE_ERR               if ::qname is malformed, or
 *                                         ::qname has a prefix and
 *                                         ::namespace is null, or ::qname
 *                                         has a prefix "xml" and
 *                                         ::namespace is not
 *                                         "http://www.w3.org/XML/1998/namespace",
 *                                         or ::qname has a prefix "xmlns"
 *                                         and ::namespace is not
 *                                         "http://www.w3.org/2000/xmlns",
 *                                         or ::namespace is
 *                                         "http://www.w3.org/2000/xmlns"
 *                                         and ::qname is not prefixed
 *                                         "xmlns",
 *         DOM_NOT_SUPPORTED_ERR           if the implementation does not
 *                                         support the feature "XML" and the
 *                                         language exposed through the
 *                                         Document does not support
 *                                         Namespaces.
 */
dom_exception _dom_element_set_attribute_ns(struct dom_element *element,
		dom_string *namespace, dom_string *qname,
		dom_string *value)
{
	dom_exception err;
	dom_string *localname;
	dom_string *prefix;

	if (_dom_validate_name(qname) == false)
		return DOM_INVALID_CHARACTER_ERR;

	err = _dom_namespace_validate_qname(qname, namespace);
	if (err != DOM_NO_ERR)
		return DOM_NAMESPACE_ERR;

	err = _dom_namespace_split_qname(qname, &prefix, &localname);
	if (err != DOM_NO_ERR)
		return err;

	/* If there is no namespace, must have a prefix */
	if (namespace == NULL && prefix != NULL) {
		dom_string_unref(prefix);
		dom_string_unref(localname);
		return DOM_NAMESPACE_ERR;
	}

	err = _dom_element_set_attr(element, namespace, localname, value);

	dom_string_unref(prefix);
	dom_string_unref(localname);

	return err;
}

/**
 * Remove an attribute from an element by namespace/localname
 *
 * \param element    The element to remove attribute from
 * \param namespace  The attribute's namespace URI
 * \param localname  The attribute's local name
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NOT_SUPPORTED_ERR           if the implementation does not
 *                                         support the feature "XML" and the
 *                                         language exposed through the
 *                                         Document does not support
 *                                         Namespaces.
 */
dom_exception _dom_element_remove_attribute_ns(struct dom_element *element,
		dom_string *namespace, dom_string *localname)
{
	return _dom_element_remove_attr(element, namespace, localname);
}

/**
 * Retrieve an attribute node from an element by namespace/localname
 *
 * \param element    The element to retrieve attribute from
 * \param namespace  The attribute's namespace URI
 * \param localname  The attribute's local name
 * \param result     Pointer to location to receive attribute node
 * \return DOM_NO_ERR            on success,
 *         DOM_NOT_SUPPORTED_ERR if the implementation does not support
 *                               the feature "XML" and the language exposed
 *                               through the Document does not support
 *                               Namespaces.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_element_get_attribute_node_ns(struct dom_element *element,
		dom_string *namespace, dom_string *localname,
		struct dom_attr **result)
{
	return _dom_element_get_attr_node(element, namespace, localname,
			result);
}

/**
 * Set an attribute node on an element, replacing existing node, if present
 *
 * \param element  The element to add a node to
 * \param attr     The attribute node to add
 * \param result   Pointer to location to recieve previous node
 * \return DOM_NO_ERR                      on success,
 *         DOM_WRONG_DOCUMENT_ERR          if ::attr does not belong to the
 *                                         same document as ::element,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_INUSE_ATTRIBUTE_ERR         if ::attr is already an attribute
 *                                         of another Element node.
 *         DOM_NOT_SUPPORTED_ERR           if the implementation does not
 *                                         support the feature "XML" and the
 *                                         language exposed through the
 *                                         Document does not support
 *                                         Namespaces.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_element_set_attribute_node_ns(struct dom_element *element,
		struct dom_attr *attr, struct dom_attr **result)
{
	dom_exception err;
	dom_string *namespace;

	err = dom_node_get_namespace(attr, (void *) &namespace);
	if (err != DOM_NO_ERR)
		return err;

	err = _dom_element_set_attr_node(element, namespace, attr, result);

	if (namespace != NULL)
		dom_string_unref(namespace);

	return err;
}

/**
 * Retrieve a list of descendant elements of an element which match a given
 * namespace/localname pair.
 *
 * \param element  The root of the subtree to search
 * \param namespace  The namespace URI to match (or "*" for all)
 * \param localname  The local name to match (or "*" for all)
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR            on success,
 *         DOM_NOT_SUPPORTED_ERR if the implementation does not support
 *                               the feature "XML" and the language exposed
 *                               through the Document does not support
 *                               Namespaces.
 *
 * The returned nodelist will have its reference count increased. It is
 * the responsibility of the caller to unref the nodelist once it has
 * finished with it.
 */
dom_exception _dom_element_get_elements_by_tag_name_ns(
		struct dom_element *element, dom_string *namespace,
		dom_string *localname, struct dom_nodelist **result)
{
	dom_exception err;

	/** \todo ensure XML feature is supported */

	err = _dom_document_get_nodelist(element->base.owner,
			DOM_NODELIST_BY_NAMESPACE,
			(struct dom_node_internal *) element, NULL, 
			namespace, localname,
			result);

	return err;
}

/**
 * Determine if an element possesses and attribute with the given name
 *
 * \param element  The element to query
 * \param name     The attribute name to look for
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_element_has_attribute(struct dom_element *element,
		dom_string *name, bool *result)
{
	return _dom_element_has_attr(element, NULL, name, result);
}

/**
 * Determine if an element possesses and attribute with the given
 * namespace/localname pair.
 *
 * \param element    The element to query
 * \param namespace  The attribute namespace URI to look for
 * \param localname  The attribute local name to look for
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR            on success,
 *         DOM_NOT_SUPPORTED_ERR if the implementation does not support
 *                               the feature "XML" and the language exposed
 *                               through the Document does not support
 *                               Namespaces.
 */
dom_exception _dom_element_has_attribute_ns(struct dom_element *element,
		dom_string *namespace, dom_string *localname,
		bool *result)
{
	return _dom_element_has_attr(element, namespace, localname, result);
}

/**
 * Retrieve the type information associated with an element
 *
 * \param element  The element to retrieve type information from
 * \param result   Pointer to location to receive type information
 * \return DOM_NO_ERR.
 *
 * The returned typeinfo will have its reference count increased. It is
 * the responsibility of the caller to unref the typeinfo once it has
 * finished with it.
 */
dom_exception _dom_element_get_schema_type_info(struct dom_element *element,
		struct dom_type_info **result)
{
	UNUSED(element);
	UNUSED(result);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * (Un)declare an attribute as being an element's ID by name
 *
 * \param element  The element containing the attribute
 * \param name     The attribute's name
 * \param is_id    Whether the attribute is an ID
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NOT_FOUND_ERR               if the specified node is not an
 *                                         attribute of ::element.
 *
 * @note: The DOM spec does not say: how to deal with when there are two or 
 * more isId attribute nodes. Here, the implementation just maintain only
 * one such attribute node.
 */
dom_exception _dom_element_set_id_attribute(struct dom_element *element,
		dom_string *name, bool is_id)
{
	return _dom_element_set_id_attr(element, NULL, name, is_id);
}

/**
 * (Un)declare an attribute as being an element's ID by namespace/localname
 *
 * \param element    The element containing the attribute
 * \param namespace  The attribute's namespace URI
 * \param localname  The attribute's local name
 * \param is_id      Whether the attribute is an ID
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NOT_FOUND_ERR               if the specified node is not an
 *                                         attribute of ::element.
 */
dom_exception _dom_element_set_id_attribute_ns(struct dom_element *element,
		dom_string *namespace, dom_string *localname,
		bool is_id)
{
	dom_exception err;

	err = _dom_element_set_id_attr(element, namespace, localname, is_id);
	
	element->id_ns = dom_string_ref(namespace);

	return err;
}

/**
 * (Un)declare an attribute node as being an element's ID
 *
 * \param element  The element containing the attribute
 * \param id_attr  The attribute node
 * \param is_id    Whether the attribute is an ID
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NOT_FOUND_ERR               if the specified node is not an
 *                                         attribute of ::element.
 */
dom_exception _dom_element_set_id_attribute_node(struct dom_element *element,
		struct dom_attr *id_attr, bool is_id)
{
	dom_exception err;
	dom_string *namespace;
	dom_string *localname;

	err = dom_node_get_namespace(id_attr, &namespace);
	if (err != DOM_NO_ERR)
		return err;
	err = dom_node_get_local_name(id_attr, &localname);
	if (err != DOM_NO_ERR)
		return err;

	err = _dom_element_set_id_attr(element, namespace, localname, is_id);
	if (err != DOM_NO_ERR)
		return err;
	
	element->id_ns = namespace;

	return DOM_NO_ERR;

}

/**
 * Obtain a pre-parsed array of class names for an element
 *
 * \param element    Element containing classes
 * \param classes    Pointer to location to receive libdom-owned array
 * \param n_classes  Pointer to location to receive number of classes
 * \return DOM_NO_ERR on success,
 *         DOM_NO_MEM_ERR on memory exhaustion
 */
dom_exception _dom_element_get_classes(struct dom_element *element,
		lwc_string ***classes, uint32_t *n_classes)
{	
	if (element->n_classes > 0) {
		uint32_t classnr;

		*classes = element->classes;
		*n_classes = element->n_classes;

		for (classnr = 0; classnr < element->n_classes; classnr++)
			lwc_string_ref((*classes)[classnr]);

	} else {
		*n_classes = 0;
		*classes = NULL;
	}

	return DOM_NO_ERR;
}

/**
 * Determine if an element has an associated class
 *
 * \param element  Element to consider
 * \param name     Class name to look for
 * \param match    Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_element_has_class(struct dom_element *element,
		lwc_string *name, bool *match)
{
	dom_exception err;
	unsigned int class;
	struct dom_node_internal *node = (struct dom_node_internal *)element;
	dom_document_quirks_mode quirks_mode;
	
	/* Short-circuit case where we have no classes */
	if (element->n_classes == 0) {
		*match = false;
		return DOM_NO_ERR;
	}

	err = dom_document_get_quirks_mode(node->owner, &quirks_mode);
	if (err != DOM_NO_ERR)
		return err;

	if (quirks_mode != DOM_DOCUMENT_QUIRKS_MODE_NONE) {
		/* Quirks mode: case insensitively match */
		for (class = 0; class < element->n_classes; class++) {
			if (lwc_error_ok == lwc_string_caseless_isequal(name,
					element->classes[class], match) &&
					*match == true)
				return DOM_NO_ERR;
		}
	} else {
		/* Standards mode: case sensitively match */
		for (class = 0; class < element->n_classes; class++) {
			if (lwc_error_ok == lwc_string_isequal(name,
					element->classes[class], match) &&
					*match == true)
				return DOM_NO_ERR;
		}
	}

	return DOM_NO_ERR;
}

/**
 * Get a named ancestor node
 *
 * \param element   Element to consider
 * \param name      Node name to look for
 * \param ancestor  Pointer to location to receive node pointer
 * \return DOM_NO_ERR.
 */
dom_exception dom_element_named_ancestor_node(dom_element *element,
		lwc_string *name, dom_element **ancestor)
{
	dom_node_internal *node = (dom_node_internal *)element;

	*ancestor = NULL;

	for (node = node->parent; node != NULL; node = node->parent) {
		if (node->type != DOM_ELEMENT_NODE)
			continue;

		assert(node->name != NULL);

		if (dom_string_caseless_lwc_isequal(node->name, name)) {
			*ancestor = (dom_element *)node;
			break;
		}
	}

	return DOM_NO_ERR;
}

/**
 * Get a named parent node
 *
 * \param element  Element to consider
 * \param name     Node name to look for
 * \param parent   Pointer to location to receive node pointer
 * \return DOM_NO_ERR.
 */
dom_exception dom_element_named_parent_node(dom_element *element,
		lwc_string *name, dom_element **parent)
{
	dom_node_internal *node = (dom_node_internal *)element;

	*parent = NULL;

	for (node = node->parent; node != NULL; node = node->parent) {
		if (node->type != DOM_ELEMENT_NODE)
			continue;

		assert(node->name != NULL);

		if (dom_string_caseless_lwc_isequal(node->name, name)) {
			*parent = (dom_element *)node;
		}
		break;
	}

	return DOM_NO_ERR;
}

/**
 * Get a named parent node
 *
 * \param element  Element to consider
 * \param name     Node name to look for
 * \param parent   Pointer to location to receive node pointer
 * \return DOM_NO_ERR.
 */
dom_exception dom_element_parent_node(dom_element *element,
		dom_element **parent)
{
	dom_node_internal *node = (dom_node_internal *)element;

	*parent = NULL;

	for (node = node->parent; node != NULL; node = node->parent) {
		if (node->type != DOM_ELEMENT_NODE)
			continue;

		*parent = (dom_element *)node;
		break;
	}

	return DOM_NO_ERR;
}

/*------------- The overload virtual functions ------------------------*/

/* Overload function of Node, please refer src/core/node.c for detail */
dom_exception _dom_element_get_attributes(dom_node_internal *node,
		struct dom_namednodemap **result)
{
	dom_exception err;
	dom_document *doc;

	doc = dom_node_get_owner(node);
	assert(doc != NULL);

	err = _dom_namednodemap_create(doc, node, &attributes_opt, result);
	if (err != DOM_NO_ERR)
		return err;
	
	dom_node_ref(node);
	
	return DOM_NO_ERR;
}

/* Overload function of Node, please refer src/core/node.c for detail */
dom_exception _dom_element_has_attributes(dom_node_internal *node, bool *result)
{
	UNUSED(node);
	*result = true;

	return DOM_NO_ERR;
}

/* For the following namespace related algorithm take a look at:
 * http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html
 */

/**
 * Look up the prefix which matches the namespace.
 *
 * \param node       The current Node in which we search for
 * \param namespace  The namespace for which we search a prefix
 * \param result     The returned prefix
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_element_lookup_prefix(dom_node_internal *node,
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

/**
 * Test whether certain namespace is the default namespace of some node.
 *
 * \param node       The Node to test
 * \param namespace  The namespace to test
 * \param result     true is the namespace is default namespace
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_element_is_default_namespace(dom_node_internal *node,
		dom_string *namespace, bool *result)
{
	struct dom_element *ele = (struct dom_element *) node;
	dom_string *value;
	dom_exception err;
	bool has;
	dom_string *xmlns;

	if (node->prefix == NULL) {
		*result = dom_string_isequal(node->namespace, namespace);
		return DOM_NO_ERR;
	}

	xmlns = _dom_namespace_get_xmlns_prefix();
	err = dom_element_has_attribute(ele, xmlns, &has);
	if (err != DOM_NO_ERR)
		return err;
	
	if (has == true) {
		err = dom_element_get_attribute(ele, xmlns, &value);
		if (err != DOM_NO_ERR)
			return err;

		*result = dom_string_isequal(value, namespace);

		dom_string_unref(value);

		return DOM_NO_ERR;
	}

	return dom_node_is_default_namespace(node->parent, namespace, result);
}

/**
 * Look up the namespace with certain prefix.
 *
 * \param node    The current node in which we search for the prefix
 * \param prefix  The prefix to search
 * \param result  The result namespace if found
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_element_lookup_namespace(dom_node_internal *node,
		dom_string *prefix, dom_string **result)
{
	dom_exception err;
	bool has;
	dom_string *xmlns;

	if (node->namespace != NULL && 
			dom_string_isequal(node->prefix, prefix)) {
		*result = dom_string_ref(node->namespace);
		return DOM_NO_ERR;
	}
	
	xmlns = _dom_namespace_get_xmlns_prefix();
	err = dom_element_has_attribute_ns(node, xmlns, prefix, &has);
	if (err != DOM_NO_ERR)
		return err;
	
	if (has == true)
		return dom_element_get_attribute_ns(node,
				dom_namespaces[DOM_NAMESPACE_XMLNS], prefix,
				result);

	err = dom_element_has_attribute(node, xmlns, &has);
	if (err != DOM_NO_ERR)
		return err;
	
	if (has == true) {
		return dom_element_get_attribute(node, xmlns, result);
	}

	return dom_node_lookup_namespace(node->parent, prefix, result);
}


/*----------------------------------------------------------------------*/
/* The protected virtual functions */

/**
 * The virtual function to parse some dom attribute
 *
 * \param ele     The element object
 * \param name    The name of the attribute
 * \param value   The new value of the attribute
 * \param parsed  The parsed value of the attribute
 * \return DOM_NO_ERR on success.
 *
 * @note: This virtual method is provided to serve as a template method. 
 * When any attribute is set or added, the attribute's value should be
 * checked to make sure that it is a valid one. And the child class of 
 * dom_element may to do some special stuff on the attribute is set. Take
 * some integer attribute as example:
 *
 * 1. The client call dom_element_set_attribute("size", "10.1"), but the
 *    size attribute may only accept an integer, and only the specific
 *    dom_element know this. And the dom_attr_set_value method, which is
 *    called by dom_element_set_attribute should call the this virtual
 *    template method.
 * 2. The overload virtual function of following one will truncate the
 *    "10.1" to "10" to make sure it is a integer. And of course, the 
 *    overload method may also save the integer as a 'int' C type for 
 *    later easy accessing by any client.
 */
dom_exception _dom_element_parse_attribute(dom_element *ele,
		dom_string *name, dom_string *value,
		dom_string **parsed)
{
	UNUSED(ele);
	UNUSED(name);

	dom_string_ref(value);
	*parsed = value;

	return DOM_NO_ERR;
}

/* The destroy virtual function of dom_element */
void __dom_element_destroy(struct dom_node_internal *node)
{
	_dom_element_destroy((struct dom_element *) node);
}

/* TODO: How to deal with default attribue:
 *
 *  Ask a language binding for default attributes.	
 *
 *	So, when we copy a element we copy all its attributes because they
 *	are all specified. For the methods like importNode and adoptNode, 
 *	this will make _dom_element_copy can be used in them.
 */
dom_exception _dom_element_copy(dom_node_internal *old, 
		dom_node_internal **copy)
{
	dom_element *olde = (dom_element *) old;
	dom_element *e;
	dom_exception err;
	uint32_t classnr;
	
	e = malloc(sizeof(dom_element));
	if (e == NULL)
		return DOM_NO_MEM_ERR;

	err = dom_node_copy_internal(old, e);
	if (err != DOM_NO_ERR) {
		free(e);
		return err;
	}

	if (olde->attributes != NULL) {
		/* Copy the attribute list */
		e->attributes = _dom_element_attr_list_clone(olde->attributes,
				e);
	} else {
		e->attributes = NULL;
	}
        
        if (olde->n_classes > 0) {
		e->n_classes = olde->n_classes;
		e->classes = malloc(sizeof(lwc_string *) * e->n_classes);
		for (classnr = 0; classnr < e->n_classes; ++classnr)
			e->classes[classnr] = 
				lwc_string_ref(olde->classes[classnr]);
        } else {
		e->n_classes = 0;
		e->classes = NULL;
        }
        
	e->id_ns = NULL;
	e->id_name = NULL;

	/* TODO: deal with dom_type_info, it get no definition ! */

	*copy = (dom_node_internal *) e;

	return DOM_NO_ERR;
}



/*--------------------------------------------------------------------------*/

/* Helper functions */

/**
 * The internal helper function for getAttribute/getAttributeNS.
 *
 * \param element    The element
 * \param namespace  The namespace to look for attribute in.  May be NULL.
 * \param name       The name of the attribute
 * \param value      The value of the attribute
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_element_get_attr(struct dom_element *element,
		dom_string *namespace, dom_string *name, dom_string **value)
{
	dom_attr_list *match;
	dom_exception err = DOM_NO_ERR;

	match = _dom_element_attr_list_find_by_name(element->attributes,
			name, namespace);

	/* Fill in value */
	if (match == NULL) {
		*value = NULL;
	} else {
		err = dom_attr_get_value(match->attr, value);
	}

	return err;
}

/**
 * The internal helper function for setAttribute and setAttributeNS.
 *
 * \param element    The element
 * \param namespace  The namespace to set attribute for.  May be NULL.
 * \param name       The name of the new attribute
 * \param value      The value of the new attribute
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_element_set_attr(struct dom_element *element,
		dom_string *namespace, dom_string *name, dom_string *value)
{
	dom_attr_list *match;
	dom_node_internal *e = (dom_node_internal *) element;
	dom_exception err;

	if (_dom_validate_name(name) == false)
		return DOM_INVALID_CHARACTER_ERR;

	/* Ensure element can be written */
	if (_dom_node_readonly(e))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	match = _dom_element_attr_list_find_by_name(element->attributes,
			name, namespace);

	if (match != NULL) {
		/* Found an existing attribute, so replace its value */

		/* Dispatch a DOMAttrModified event */
		dom_string *old = NULL;
		struct dom_document *doc = dom_node_get_owner(element);
		bool success = true;
		err = dom_attr_get_value(match->attr, &old);
		/* TODO: We did not support some node type such as entity
		 * reference, in that case, we should ignore the error to
		 * make sure the event model work as excepted. */
		if (err != DOM_NO_ERR && err != DOM_NOT_SUPPORTED_ERR)
			return err;
		err = _dom_dispatch_attr_modified_event(doc, e, old, value,
				match->attr, name, DOM_MUTATION_MODIFICATION,
				&success);
		dom_string_unref(old);
		if (err != DOM_NO_ERR)
			return err;

		success = true;
		err = _dom_dispatch_subtree_modified_event(doc,
				(dom_event_target *) e, &success);
		if (err != DOM_NO_ERR)
			return err;

		err = dom_attr_set_value(match->attr, value);
		if (err != DOM_NO_ERR)
			return err;
	} else {
		/* No existing attribute, so create one */
		struct dom_attr *attr;
		struct dom_attr_list *list_node;
		struct dom_document *doc;
		bool success = true;

		err = _dom_attr_create(e->owner, name, namespace, NULL,
				true, &attr);
		if (err != DOM_NO_ERR)
			return err;

		/* Set its parent, so that value parsing works */
		dom_node_set_parent(attr, element);

		/* Set its value */
		err = dom_attr_set_value(attr, value);
		if (err != DOM_NO_ERR) {
			dom_node_set_parent(attr, NULL);
			dom_node_unref(attr);
			return err;
		}

		/* Dispatch a DOMAttrModified event */
		doc = dom_node_get_owner(element);
		err = _dom_dispatch_attr_modified_event(doc, e, NULL, value,
				(dom_event_target *) attr, name, 
				DOM_MUTATION_ADDITION, &success);
		if (err != DOM_NO_ERR) {
			dom_node_set_parent(attr, NULL);
			dom_node_unref(attr);
			return err;
		}

		err = dom_node_dispatch_node_change_event(doc,
				attr, element, DOM_MUTATION_ADDITION, &success);
		if (err != DOM_NO_ERR) {
			dom_node_set_parent(attr, NULL);
			dom_node_unref(attr);
			return err;
		}

		/* Create attribute list node */
		list_node = _dom_element_attr_list_node_create(attr, element,
				name, namespace);
		if (list_node == NULL) {
			/* If we failed at this step, there must be no memory */
			dom_node_set_parent(attr, NULL);
			dom_node_unref(attr);
			return DOM_NO_MEM_ERR;
		}
		dom_string_ref(name);
		dom_string_ref(namespace);

		/* Link into element's attribute list */
		if (element->attributes == NULL)
			element->attributes = list_node;
		else
			_dom_element_attr_list_insert(element->attributes,
					list_node);

		dom_node_unref(attr);
		dom_node_remove_pending(attr);

		success = true;
		err = _dom_dispatch_subtree_modified_event(doc,
				(dom_event_target *) element, &success);
		if (err != DOM_NO_ERR)
			return err;
	}

	return DOM_NO_ERR;
}

/**
 * Remove an attribute from an element by name
 *
 * \param element  The element to remove attribute from
 * \param name     The name of the attribute to remove
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly.
 */
dom_exception _dom_element_remove_attr(struct dom_element *element,
		dom_string *namespace, dom_string *name)
{
	dom_attr_list *match;
	dom_exception err;
	dom_node_internal *e = (dom_node_internal *) element;

	/* Ensure element can be written to */
	if (_dom_node_readonly(e))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	match = _dom_element_attr_list_find_by_name(element->attributes,
			name, namespace);

	/* Detach attr node from list */
	if (match != NULL) {
		/* Disptach DOMNodeRemoval event */
		bool success = true;
		dom_attr *a = match->attr;
		struct dom_document *doc = dom_node_get_owner(element);
		dom_string *old = NULL;

		err = dom_node_dispatch_node_change_event(doc, match->attr,
				element, DOM_MUTATION_REMOVAL, &success);
		if (err != DOM_NO_ERR)
			return err;

		/* Claim a reference for later event dispatch */
		dom_node_ref(a);


		/* Delete the attribute node */
		if (element->attributes == match) {
			element->attributes =
					_dom_element_attr_list_next(match);
		}
		if (element->attributes == match) {
			/* match must be sole attribute */
			element->attributes = NULL;
		}
		_dom_element_attr_list_node_unlink(match);
		_dom_element_attr_list_node_destroy(match);

		/* Dispatch a DOMAttrModified event */
		success = true;
		err = dom_attr_get_value(a, &old);
		/* TODO: We did not support some node type such as entity
		 * reference, in that case, we should ignore the error to
		 * make sure the event model work as excepted. */
		if (err != DOM_NO_ERR && err != DOM_NOT_SUPPORTED_ERR)
			return err;
		err = _dom_dispatch_attr_modified_event(doc, e, old, NULL, a,
				name, DOM_MUTATION_REMOVAL, &success);
		dom_string_unref(old);
		/* Release the reference */
		dom_node_unref(a);
		if (err != DOM_NO_ERR)
			return err;

		success = true;
		err = _dom_dispatch_subtree_modified_event(doc,
				(dom_event_target *) e, &success);
		if (err != DOM_NO_ERR)
			return err;
	}

	/** \todo defaulted attribute handling */

	return DOM_NO_ERR;
}

/**
 * Retrieve an attribute node from an element by name
 *
 * \param element  The element to retrieve attribute node from
 * \param name     The attribute's name
 * \param result   Pointer to location to receive attribute node
 * \return DOM_NO_ERR.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_element_get_attr_node(struct dom_element *element,
		dom_string *namespace, dom_string *name,
		struct dom_attr **result)
{
	dom_attr_list *match;

	match = _dom_element_attr_list_find_by_name(element->attributes,
			name, namespace);

	/* Fill in value */
	if (match == NULL) {
		*result = NULL;
	} else {
		*result = match->attr;
		dom_node_ref(*result);
	}

	return DOM_NO_ERR;
}

/**
 * Set an attribute node on an element, replacing existing node, if present
 *
 * \param element  The element to add a node to
 * \param attr     The attribute node to add
 * \param result   Pointer to location to receive previous node
 * \return DOM_NO_ERR                      on success,
 *         DOM_WRONG_DOCUMENT_ERR          if ::attr does not belong to the
 *                                         same document as ::element,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_INUSE_ATTRIBUTE_ERR         if ::attr is already an attribute
 *                                         of another Element node.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_element_set_attr_node(struct dom_element *element,
		dom_string *namespace, struct dom_attr *attr,
		struct dom_attr **result)
{
	dom_attr_list *match;
	dom_exception err;
	dom_string *name = NULL;
	dom_node_internal *e = (dom_node_internal *) element;
	dom_node_internal *attr_node = (dom_node_internal *) attr;
	dom_attr *old_attr;
	dom_string *new = NULL;
	struct dom_document *doc;
	bool success = true;

	/** \todo validate name */

	/* Ensure element and attribute belong to the same document */
	if (e->owner != attr_node->owner)
		return DOM_WRONG_DOCUMENT_ERR;

	/* Ensure element can be written to */
	if (_dom_node_readonly(e))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	/* Ensure attribute isn't attached to another element */
	if (attr_node->parent != NULL && attr_node->parent != e)
		return DOM_INUSE_ATTRIBUTE_ERR;

	err = dom_node_get_local_name(attr, &name);
	if (err != DOM_NO_ERR)
		return err;

	match = _dom_element_attr_list_find_by_name(element->attributes,
			name, namespace);

	*result = NULL;
	if (match != NULL) {
		/* Disptach DOMNodeRemoval event */
		dom_string *old = NULL;
		doc = dom_node_get_owner(element);
		old_attr = match->attr;

		err = dom_node_dispatch_node_change_event(doc, old_attr, 
				element, DOM_MUTATION_REMOVAL, &success);
		if (err != DOM_NO_ERR) {
			dom_string_unref(name);
			return err;
		}

		dom_node_ref(old_attr);

		_dom_element_attr_list_node_unlink(match);
		_dom_element_attr_list_node_destroy(match);

		/* Dispatch a DOMAttrModified event */
		success = true;
		err = dom_attr_get_value(old_attr, &old);
		/* TODO: We did not support some node type such as entity
		 * reference, in that case, we should ignore the error to
		 * make sure the event model work as excepted. */
		if (err != DOM_NO_ERR && err != DOM_NOT_SUPPORTED_ERR) {
			dom_node_unref(old_attr);
			dom_string_unref(name);
			return err;
		}
		err = _dom_dispatch_attr_modified_event(doc, e, old, NULL, 
				(dom_event_target *) old_attr, name, 
				DOM_MUTATION_REMOVAL, &success);
		dom_string_unref(old);
		*result = old_attr;
		if (err != DOM_NO_ERR) {
			dom_string_unref(name);
			return err;
		}

		success = true;
		err = _dom_dispatch_subtree_modified_event(doc,
				(dom_event_target *) e, &success);
		if (err != DOM_NO_ERR) {
			dom_string_unref(name);
			return err;
		}
	}


	match = _dom_element_attr_list_node_create(attr, element,
			name, namespace);
	if (match == NULL) {
		dom_string_unref(name);
		/* If we failed at this step, there must be no memory */
		return DOM_NO_MEM_ERR;
	}

	dom_string_ref(name);
	dom_string_ref(namespace);
	dom_node_set_parent(attr, element);
	dom_node_remove_pending(attr);

	/* Dispatch a DOMAttrModified event */
	doc = dom_node_get_owner(element);
	success = true;
	err = dom_attr_get_value(attr, &new);
	/* TODO: We did not support some node type such as entity reference, in
	 * that case, we should ignore the error to make sure the event model
	 * work as excepted. */
	if (err != DOM_NO_ERR && err != DOM_NOT_SUPPORTED_ERR) {
		_dom_element_attr_list_node_destroy(match);
		return err;
	}
	err = _dom_dispatch_attr_modified_event(doc, e, NULL, new,
			(dom_event_target *) attr, name, 
			DOM_MUTATION_ADDITION, &success);
	/* Cleanup */
	dom_string_unref(new);
	dom_string_unref(name);
	if (err != DOM_NO_ERR) {
		_dom_element_attr_list_node_destroy(match);
		return err;
	}

	err = dom_node_dispatch_node_change_event(doc, attr, element, 
			DOM_MUTATION_ADDITION, &success);
	if (err != DOM_NO_ERR) {
		_dom_element_attr_list_node_destroy(match);
		return err;
	}

	success = true;
	err = _dom_dispatch_subtree_modified_event(doc,
			(dom_event_target *) element, &success);
	if (err != DOM_NO_ERR) {
		_dom_element_attr_list_node_destroy(match);
		return err;
	}

	/* Link into element's attribute list */
	if (element->attributes == NULL)
		element->attributes = match;
	else
		_dom_element_attr_list_insert(element->attributes, match);

	return DOM_NO_ERR;
}

/**
 * Remove an attribute node from an element
 *
 * \param element  The element to remove attribute node from
 * \param attr     The attribute node to remove
 * \param result   Pointer to location to receive attribute node
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NOT_FOUND_ERR               if ::attr is not an attribute of
 *                                         ::element.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_element_remove_attr_node(struct dom_element *element,
		dom_string *namespace, struct dom_attr *attr,
		struct dom_attr **result)
{
	dom_attr_list *match;
	dom_exception err;
	dom_string *name;
	dom_node_internal *e = (dom_node_internal *) element;
	dom_attr *a;
	bool success = true;
	struct dom_document *doc;
	dom_string *old = NULL;
	
	/* Ensure element can be written to */
	if (_dom_node_readonly(e))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;
	
	err = dom_node_get_node_name(attr, &name);
	if (err != DOM_NO_ERR)
		return err;

	match = _dom_element_attr_list_find_by_name(element->attributes,
			name, namespace);

	/** \todo defaulted attribute handling */

	if (match == NULL || match->attr != attr) {
		dom_string_unref(name);
		return DOM_NOT_FOUND_ERR;
	}

	a = match->attr;

	/* Dispatch a DOMNodeRemoved event */
	doc = dom_node_get_owner(element);
	err = dom_node_dispatch_node_change_event(doc, a, element, 
			DOM_MUTATION_REMOVAL, &success);
	if (err != DOM_NO_ERR) {
		dom_string_unref(name);
		return err;
	}

	dom_node_ref(a);

	/* Delete the attribute node */
	if (element->attributes == match) {
		element->attributes = _dom_element_attr_list_next(match);
	}
	if (element->attributes == match) {
		/* match must be sole attribute */
		element->attributes = NULL;
	}
	_dom_element_attr_list_node_unlink(match);
	_dom_element_attr_list_node_destroy(match);

	/* Now, cleaup the dom_string */
	dom_string_unref(name);

	/* Dispatch a DOMAttrModified event */
	success = true;
	err = dom_attr_get_value(a, &old);
	/* TODO: We did not support some node type such as entity reference, in
	 * that case, we should ignore the error to make sure the event model
	 * work as excepted. */
	if (err != DOM_NO_ERR && err != DOM_NOT_SUPPORTED_ERR) {
		dom_node_unref(a);
		return err;
	}
	err = _dom_dispatch_attr_modified_event(doc, e, old, NULL, 
			(dom_event_target *) a, name, 
			DOM_MUTATION_REMOVAL, &success);
	dom_string_unref(old);
	if (err != DOM_NO_ERR)
		return err;

	/* When a Node is removed, it should be destroy. When its refcnt is not 
	 * zero, it will be added to the document's deletion pending list. 
	 * When a Node is removed, its parent should be NULL, but its owner
	 * should remain to be the document.
	 */
	*result = (dom_attr *) a;

	success = true;
	err = _dom_dispatch_subtree_modified_event(doc,
			(dom_event_target *) e, &success);
	if (err != DOM_NO_ERR)
		return err;

	return DOM_NO_ERR;
}

/**
 * Test whether certain attribute exists on the element
 *
 * \param element    The element
 * \param namespace  The namespace to look for attribute in.  May be NULL.
 * \param name       The attribute's name
 * \param result     The return value
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_element_has_attr(struct dom_element *element,
		dom_string *namespace, dom_string *name, bool *result)
{
	dom_attr_list *match;

	match = _dom_element_attr_list_find_by_name(element->attributes,
			name, namespace);

	/* Fill in result */
	if (match == NULL) {
		*result = false;
	} else {
		*result = true;
	}

	return DOM_NO_ERR;
}

/**
 * (Un)set an attribute Node as a ID.
 *
 * \param element    The element contains the attribute
 * \param namespace  The namespace of the attribute node
 * \param name       The name of the attribute
 * \param is_id      true for set the node as a ID attribute, false unset it
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_element_set_id_attr(struct dom_element *element,
		dom_string *namespace, dom_string *name, bool is_id)
{
	
	dom_attr_list *match;

	match = _dom_element_attr_list_find_by_name(element->attributes,
			name, namespace);
	if (match == NULL)
		return DOM_NOT_FOUND_ERR;
	
	if (is_id == true) {
		/* Clear the previous id attribute if there is one */
		dom_attr_list *old = _dom_element_attr_list_find_by_name(
				element->attributes, element->id_name,
				element->id_ns);

		if (old != NULL) {
			_dom_attr_set_isid(old->attr, false);
		}

		/* Set up the new id attr stuff */
		element->id_name = dom_string_ref(name);
		element->id_ns = dom_string_ref(namespace);
	}

	_dom_attr_set_isid(match->attr, is_id);

	return DOM_NO_ERR;
}

/**
 * Get the ID string of the element
 *
 * \param ele  The element
 * \param id   The ID of this element
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_element_get_id(struct dom_element *ele, dom_string **id)
{
	dom_exception err;
	dom_string *ret = NULL;
	dom_document *doc;
	dom_string *name;

	*id = NULL;

	if (ele->id_ns != NULL && ele->id_name != NULL) {
		/* There is user specific ID attribute */
		err = _dom_element_get_attribute_ns(ele, ele->id_ns, 
				ele->id_name, &ret);
		if (err != DOM_NO_ERR) {
			return err;
		}

		*id = ret;
		return err;
	}

	doc = dom_node_get_owner(ele);
	assert(doc != NULL);

	if (ele->id_name != NULL) {
		name = ele->id_name;
	} else {
		name = _dom_document_get_id_name(doc);

		if (name == NULL) {
			/* No ID attribute at all, just return NULL */
			*id = NULL;
			return DOM_NO_ERR;
		}
	}

	err = _dom_element_get_attribute(ele, name, &ret);
	if (err != DOM_NO_ERR) {
		return err;
	}

	if (ret != NULL) {
		*id = ret;
	} else {
		*id = NULL;
	}

	return err;
}



/*-------------- The dom_namednodemap functions -------------------------*/

/* Implementation function for NamedNodeMap, see core/namednodemap.h for 
 * details */
dom_exception attributes_get_length(void *priv, uint32_t *length)
{
	dom_element *e = (dom_element *) priv;

	*length = _dom_element_attr_list_length(e->attributes);

	return DOM_NO_ERR;
}

/* Implementation function for NamedNodeMap, see core/namednodemap.h for 
 * details */
dom_exception attributes_get_named_item(void *priv,
		dom_string *name, struct dom_node **node)
{
	dom_element *e = (dom_element *) priv;

	return _dom_element_get_attribute_node(e, name, (dom_attr **) node);
}

/* Implementation function for NamedNodeMap, see core/namednodemap.h for 
 * details */
dom_exception attributes_set_named_item(void *priv,
		struct dom_node *arg, struct dom_node **node)
{
	dom_element *e = (dom_element *) priv;
	dom_node_internal *n = (dom_node_internal *) arg;

	if (n->type != DOM_ATTRIBUTE_NODE)
		return DOM_HIERARCHY_REQUEST_ERR;

	return _dom_element_set_attribute_node(e, (dom_attr *) arg,
			(dom_attr **) node);
}

/* Implementation function for NamedNodeMap, see core/namednodemap.h for 
 * details */
dom_exception attributes_remove_named_item(
		void *priv, dom_string *name,
		struct dom_node **node)
{
	dom_element *e = (dom_element *) priv;
	dom_exception err;

	err = _dom_element_get_attribute_node(e, name, (dom_attr **) node);
	if (err != DOM_NO_ERR)
		return err;

	if (*node == NULL) {
		return DOM_NOT_FOUND_ERR;
	}
	
	return _dom_element_remove_attribute(e, name);
}

/* Implementation function for NamedNodeMap, see core/namednodemap.h for 
 * details */
dom_exception attributes_item(void *priv,
		uint32_t index, struct dom_node **node)
{
	dom_attr_list * match = NULL;
	unsigned int num = index + 1;
	dom_element *e = (dom_element *) priv;

	match = _dom_element_attr_list_get_by_index(e->attributes, num);

	if (match != NULL) {
		*node = (dom_node *) match->attr;
		dom_node_ref(*node);
	} else {
		*node = NULL;
	}

	return DOM_NO_ERR;
}

/* Implementation function for NamedNodeMap, see core/namednodemap.h for 
 * details */
dom_exception attributes_get_named_item_ns(
		void *priv, dom_string *namespace,
		dom_string *localname, struct dom_node **node)
{
	dom_element *e = (dom_element *) priv;

	return _dom_element_get_attribute_node_ns(e, namespace, localname,
			(dom_attr **) node);
}

/* Implementation function for NamedNodeMap, see core/namednodemap.h for 
 * details */
dom_exception attributes_set_named_item_ns(
		void *priv, struct dom_node *arg,
		struct dom_node **node)
{
	dom_element *e = (dom_element *) priv;
	dom_node_internal *n = (dom_node_internal *) arg;

	if (n->type != DOM_ATTRIBUTE_NODE)
		return DOM_HIERARCHY_REQUEST_ERR;

	return _dom_element_set_attribute_node_ns(e, (dom_attr *) arg,
			(dom_attr **) node);
}

/* Implementation function for NamedNodeMap, see core/namednodemap.h for 
 * details */
dom_exception attributes_remove_named_item_ns(
		void *priv, dom_string *namespace,
		dom_string *localname, struct dom_node **node)
{
	dom_element *e = (dom_element *) priv;
	dom_exception err;
	
	err = _dom_element_get_attribute_node_ns(e, namespace, localname,
			(dom_attr **) node);
	if (err != DOM_NO_ERR)
		return err;

	if (*node == NULL) {
		return DOM_NOT_FOUND_ERR;
	}

	return _dom_element_remove_attribute_ns(e, namespace, localname);
}

/* Implementation function for NamedNodeMap, see core/namednodemap.h for 
 * details */
void attributes_destroy(void *priv)
{
	dom_element *e = (dom_element *) priv;

	dom_node_unref(e);
}

/* Implementation function for NamedNodeMap, see core/namednodemap.h for 
 * details */
bool attributes_equal(void *p1, void *p2)
{
	/* We have passed the pointer to this element as the private data,
	 * and here we just need to compare whether the two elements are 
	 * equal
	 */
	return p1 == p2;
}
/*------------------ End of namednodemap functions -----------------------*/

