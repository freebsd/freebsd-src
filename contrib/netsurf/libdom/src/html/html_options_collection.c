/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdlib.h>

#include <libwapcaplet/libwapcaplet.h>

#include "html/html_options_collection.h"

#include "core/node.h"
#include "core/element.h"
#include "core/string.h"
#include "utils/utils.h"

/*-----------------------------------------------------------------------*/
/* Constructor and destructor */

/**
 * Create a dom_html_options_collection
 *
 * \param doc   The document
 * \param root  The root element of the collection
 * \param ic    The callback function used to determin whether certain node
 *              beint32_ts to the collection
 * \param col   The result collection object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_options_collection_create(struct dom_html_document *doc,
		struct dom_node_internal *root,
		dom_callback_is_in_collection ic,
		void *ctx,
		struct dom_html_options_collection **col)
{
	*col = malloc(sizeof(dom_html_options_collection));
	if (*col == NULL)
		return DOM_NO_MEM_ERR;
	
	return _dom_html_options_collection_initialise(doc, *col, root,
						       ic, ctx);
}

/**
 * Intialiase a dom_html_options_collection
 *
 * \param doc   The document
 * \param col   The collection object to be initialised
 * \param root  The root element of the collection
 * \param ic    The callback function used to determin whether certain node
 *              beint32_ts to the collection
 * \return DOM_NO_ERR on success.
 */
dom_exception _dom_html_options_collection_initialise(struct dom_html_document *doc,
		struct dom_html_options_collection *col,
		struct dom_node_internal *root,
		dom_callback_is_in_collection ic, void *ctx)
{
	return _dom_html_collection_initialise(doc, &col->base, root, ic, ctx);
}

/**
 * Finalise a dom_html_options_collection object
 *
 * \param col  The dom_html_options_collection object
 */
void _dom_html_options_collection_finalise(struct dom_html_options_collection *col)
{
	_dom_html_collection_finalise(&col->base);
}

/**
 * Destroy a dom_html_options_collection object
 * \param col  The dom_html_options_collection object
 */
void _dom_html_options_collection_destroy(struct dom_html_options_collection *col)
{
	_dom_html_options_collection_finalise(col);

	free(col);
}


/*-----------------------------------------------------------------------*/
/* Public API */

/**
 * Get the length of this dom_html_options_collection
 *
 * \param col  The dom_html_options_collection object
 * \param len  The returned length of this collection
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_options_collection_get_length(dom_html_options_collection *col,
		uint32_t *len)
{
	return dom_html_collection_get_length(&col->base, len);
}

/**
 * Set the length of this dom_html_options_collection
 *
 * \param col  The dom_html_options_collection object
 * \param len  The length of this collection to be set
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_options_collection_set_length(
		dom_html_options_collection *col, uint32_t len)
{
	UNUSED(col);
	UNUSED(len);

	/* TODO: how to deal with this */
	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Get the node with certain index
 *
 * \param col  The dom_html_options_collection object
 * \param index  The index number based on zero
 * \param node   The returned node object
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_options_collection_item(dom_html_options_collection *col,
		uint32_t index, struct dom_node **node)
{
	return dom_html_collection_item(&col->base, index, node);
}

/**
 * Get the node in the collection according name
 *
 * \param col   The collection
 * \param name  The name of target node
 * \param node  The returned node object
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_options_collection_named_item(dom_html_options_collection *col,
		dom_string *name, struct dom_node **node)
{
	struct dom_node_internal *n = col->base.root;
	dom_string *kname;
	dom_exception err;

	/* Search for an element with an appropriate ID */
	err = dom_html_collection_named_item(&col->base, name, node);
	if (err == DOM_NO_ERR && *node != NULL)
		return err;

	/* Didn't find one, so consider name attribute */
	err = dom_string_create_interned((const uint8_t *) "name", SLEN("name"),
			&kname);
	if (err != DOM_NO_ERR)
		return err;

	while (n != NULL) {
		if (n->type == DOM_ELEMENT_NODE &&
		    col->base.ic(n, col->base.ctx) == true) {
			dom_string *nval = NULL;

			err = dom_element_get_attribute(n, kname, &nval);
			if (err != DOM_NO_ERR) {
				dom_string_unref(kname);
				return err;
			}

			if (nval != NULL && dom_string_isequal(name, nval)) {
				*node = (struct dom_node *) n;
				dom_node_ref(n);
				dom_string_unref(nval);
				dom_string_unref(kname);

				return DOM_NO_ERR;
			}

			if (nval != NULL)
				dom_string_unref(nval);
		}

		/* Depth first iterating */
		if (n->first_child != NULL) {
			n = n->first_child;
		} else if (n->next != NULL) {
			n = n->next;
		} else {
			/* No children and siblings */
			struct dom_node_internal *parent = n->parent;

			while (parent != col->base.root &&
					n == parent->last_child) {
				n = parent;
				parent = parent->parent;
			}
			
			if (parent == col->base.root)
				n = NULL;
			else
				n = n->next;
		}
	}

	dom_string_unref(kname);

	/* Not found the target node */
	*node = NULL;

	return DOM_NO_ERR;
}

/**
 * Claim a reference on this collection
 *
 * \pram col  The collection object
 */
void dom_html_options_collection_ref(dom_html_options_collection *col)
{
	if (col == NULL)
		return;
	
	col->base.refcnt ++;
}

/**
 * Relese a reference on this collection
 *
 * \pram col  The collection object
 */
void dom_html_options_collection_unref(dom_html_options_collection *col)
{
	if (col == NULL)
		return;
	
	if (col->base.refcnt > 0)
		col->base.refcnt --;
	
	if (col->base.refcnt == 0)
		_dom_html_options_collection_destroy(col);
}

