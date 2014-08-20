/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku.com>
 */

#include <stdlib.h>

#include "html/html_document.h"
#include "html/html_html_element.h"

#include "core/node.h"
#include "utils/utils.h"

static struct dom_element_protected_vtable _protect_vtable = {
	{
		DOM_NODE_PROTECT_VTABLE_HTML_HTML_ELEMENT
	},
	DOM_HTML_HTML_ELEMENT_PROTECT_VTABLE
};

/**
 * Create a dom_html_html_element object
 *
 * \param doc  The document object
 * \param ele  The returned element object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_html_element_create(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,                            
		struct dom_html_html_element **ele)
{
	struct dom_node_internal *node;

	*ele = malloc(sizeof(dom_html_html_element));
	if (*ele == NULL)
		return DOM_NO_MEM_ERR;
	
	/* Set up vtables */
	node = (struct dom_node_internal *) *ele;
	node->base.vtable = &_dom_html_element_vtable;
	node->vtable = &_protect_vtable;

	return _dom_html_html_element_initialise(doc, namespace, prefix, *ele);
}

/**
 * Initialise a dom_html_html_element object
 *
 * \param doc  The document object
 * \param ele  The dom_html_html_element object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_html_element_initialise(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,                            
		struct dom_html_html_element *ele)
{
	return _dom_html_element_initialise(doc, &ele->base,
			doc->memoised[hds_HTML], namespace, prefix);
}

/**
 * Finalise a dom_html_html_element object
 *
 * \param ele  The dom_html_html_element object
 */
void _dom_html_html_element_finalise(struct dom_html_html_element *ele)
{
	_dom_html_element_finalise(&ele->base);
}

/**
 * Destroy a dom_html_html_element object
 *
 * \param ele  The dom_html_html_element object
 */
void _dom_html_html_element_destroy(struct dom_html_html_element *ele)
{
	_dom_html_html_element_finalise(ele);

	free(ele);
}

/*------------------------------------------------------------------------*/
/* The protected virtual functions */

/* The virtual function used to parse attribute value, see src/core/element.c
 * for detail */
dom_exception _dom_html_html_element_parse_attribute(dom_element *ele,
		dom_string *name, dom_string *value,
		dom_string **parsed)
{
	UNUSED(ele);
	UNUSED(name);

	dom_string_ref(value);
	*parsed = value;

	return DOM_NO_ERR;
}

/* The virtual destroy function, see src/core/node.c for detail */
void _dom_virtual_html_html_element_destroy(dom_node_internal *node)
{
	_dom_html_html_element_destroy((struct dom_html_html_element *) node);
}

/* The virtual copy function, see src/core/node.c for detail */
dom_exception _dom_html_html_element_copy(dom_node_internal *old,
		dom_node_internal **copy)
{
	return _dom_html_element_copy(old, copy);
}

/*-----------------------------------------------------------------------*/
/* API functions */

dom_exception dom_html_html_element_get_version(dom_html_html_element *element,
					   dom_string **version)
{
	dom_exception ret;
	dom_string *_memo_version;

	_memo_version =
		((struct dom_html_document *)
		 ((struct dom_node_internal *)element)->owner)->memoised[hds_version];

	ret = dom_element_get_attribute(element, _memo_version, version);

	return ret;
}

dom_exception dom_html_html_element_set_version(dom_html_html_element *element,
					   dom_string *version)
{
	dom_exception ret;
	dom_string *_memo_version;

	_memo_version =
		((struct dom_html_document *)
		 ((struct dom_node_internal *)element)->owner)->memoised[hds_version];

	ret = dom_element_set_attribute(element, _memo_version, version);

	return ret;
}
