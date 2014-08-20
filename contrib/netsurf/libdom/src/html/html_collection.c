/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdlib.h>

#include <libwapcaplet/libwapcaplet.h>

#include "html/html_collection.h"

#include "core/node.h"
#include "core/element.h"
#include "core/string.h"

/*-----------------------------------------------------------------------*/
/* Constructor and destructor */

/**
 * Create a dom_html_collection
 *
 * \param doc   The document
 * \param root  The root element of the collection
 * \param ic    The callback function used to determin whether certain node
 *              beint32_ts to the collection
 * \param col   The result collection object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_collection_create(struct dom_html_document *doc,
		struct dom_node_internal *root,
		dom_callback_is_in_collection ic,
		void *ctx,
		struct dom_html_collection **col)
{
	*col = malloc(sizeof(dom_html_collection));
	if (*col == NULL)
		return DOM_NO_MEM_ERR;
	
	return _dom_html_collection_initialise(doc, *col, root, ic, ctx);
}

/**
 * Intialiase a dom_html_collection
 *
 * \param doc   The document
 * \param col   The collection object to be initialised
 * \param root  The root element of the collection
 * \param ic    The callback function used to determin whether certain node
 *              beint32_ts to the collection
 * \return DOM_NO_ERR on success.
 */
dom_exception _dom_html_collection_initialise(struct dom_html_document *doc,
		struct dom_html_collection *col,
		struct dom_node_internal *root,
		dom_callback_is_in_collection ic, void *ctx)
{
	assert(doc != NULL);
	assert(ic != NULL);
	assert(root != NULL);

	col->doc = doc;
	dom_node_ref(doc);

	col->root = root;
	dom_node_ref(root);

	col->ic = ic;
	col->ctx = ctx;
	col->refcnt = 1;

	return DOM_NO_ERR;
}

/**
 * Finalise a dom_html_collection object
 *
 * \param col  The dom_html_collection object
 */
void _dom_html_collection_finalise(struct dom_html_collection *col)
{
	dom_node_unref(col->doc);
	col->doc = NULL;

	dom_node_unref(col->root);
	col->root = NULL;

	col->ic = NULL;
}

/**
 * Destroy a dom_html_collection object
 * \param col  The dom_html_collection object
 */
void _dom_html_collection_destroy(struct dom_html_collection *col)
{
	_dom_html_collection_finalise(col);

	free(col);
}


/*-----------------------------------------------------------------------*/
/* Public API */

/**
 * Get the length of this dom_html_collection
 *
 * \param col  The dom_html_collection object
 * \param len  The returned length of this collection
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_collection_get_length(dom_html_collection *col,
		uint32_t *len)
{
	struct dom_node_internal *node = col->root;
	*len = 0;

	while (node != NULL) {
		if (node->type == DOM_ELEMENT_NODE && 
		    col->ic(node, col->ctx) == true)
			(*len)++;

		/* Depth first iterating */
		if (node->first_child != NULL) {
			node = node->first_child;
		} else if (node->next != NULL) {
			node = node->next;
		} else {
			/* No children and siblings */
			struct dom_node_internal *parent = node->parent;

			while (parent != col->root &&
					node == parent->last_child) {
				node = parent;
				parent = parent->parent;
			}
			
			if (node == col->root)
				node = NULL;
			else
				node = node->next;
		}
	}

	return DOM_NO_ERR;
}

/**
 * Get the node with certain index
 *
 * \param col  The dom_html_collection object
 * \param index  The index number based on zero
 * \param node   The returned node object
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_collection_item(dom_html_collection *col,
		uint32_t index, struct dom_node **node)
{
	struct dom_node_internal *n = col->root;
	uint32_t len = 0;

	while (n != NULL) {
		if (n->type == DOM_ELEMENT_NODE && 
		    col->ic(n, col->ctx) == true)
			len++;

		if (len == index + 1) {
			dom_node_ref(n);
			*node = (struct dom_node *) n;
			return DOM_NO_ERR;
		}

		/* Depth first iterating */
		if (n->first_child != NULL) {
			n = n->first_child;
		} else if (n->next != NULL) {
			n = n->next;
		} else {
			/* No children and siblings */
			struct dom_node_internal *parent = n->parent;

			while (parent != col->root &&
					n == parent->last_child) {
				n = parent;
				parent = parent->parent;
			}
			
			if (n == col->root)
				n = NULL;
			else
				n = n->next;
		}
	}

	/* Not find the node */
	*node = NULL;
	return DOM_NO_ERR;
}

/**
 * Get the node in the collection according name
 *
 * \param col   The collection
 * \param name  The name of target node
 * \param node  The returned node object
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_collection_named_item(dom_html_collection *col,
		dom_string *name, struct dom_node **node)
{
	struct dom_node_internal *n = col->root;
	dom_exception err;
        
        while (n != NULL) {
		if (n->type == DOM_ELEMENT_NODE &&
		    col->ic(n, col->ctx) == true) {
			dom_string *id = NULL;

			err = _dom_element_get_id((struct dom_element *) n,
					&id);
			if (err != DOM_NO_ERR) {
				return err;
			}

			if (id != NULL && dom_string_isequal(name, id)) {
				*node = (struct dom_node *) n;
				dom_node_ref(n);
				dom_string_unref(id);

				return DOM_NO_ERR;
			}

			if (id != NULL)
				dom_string_unref(id);
		}

		/* Depth first iterating */
		if (n->first_child != NULL) {
			n = n->first_child;
		} else if (n->next != NULL) {
			n = n->next;
		} else {
			/* No children and siblings */
			struct dom_node_internal *parent = n->parent;

			while (parent != col->root &&
					n == parent->last_child) {
				n = parent;
				parent = parent->parent;
			}
			
			if (parent == col->root)
				n = NULL;
			else
				n = n->next;
		}
	}

	/* Not found the target node */
	*node = NULL;

	return DOM_NO_ERR;
}

/**
 * Claim a reference on this collection
 *
 * \pram col  The collection object
 */
void dom_html_collection_ref(dom_html_collection *col)
{
	if (col == NULL)
		return;
	
	col->refcnt ++;
}

/**
 * Relese a reference on this collection
 *
 * \pram col  The collection object
 */
void dom_html_collection_unref(dom_html_collection *col)
{
	if (col == NULL)
		return;
	
	if (col->refcnt > 0)
		col->refcnt --;
	
	if (col->refcnt == 0)
		_dom_html_collection_destroy(col);
}

