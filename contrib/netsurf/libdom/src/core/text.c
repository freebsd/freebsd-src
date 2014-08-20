/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdlib.h>

#include <dom/core/string.h>
#include <dom/core/text.h>

#include <libwapcaplet/libwapcaplet.h>

#include "core/characterdata.h"
#include "core/document.h"
#include "core/text.h"
#include "utils/utils.h"

/* The virtual table for dom_text */
struct dom_text_vtable text_vtable = {
	{
		{
			{
				DOM_NODE_EVENT_TARGET_VTABLE,
			},
			DOM_NODE_VTABLE_CHARACTERDATA
		},
		DOM_CHARACTERDATA_VTABLE
	},
	DOM_TEXT_VTABLE
};

static struct dom_node_protect_vtable text_protect_vtable = {
	DOM_TEXT_PROTECT_VTABLE
};

/* Following comes helper functions */
typedef enum walk_operation {
	COLLECT,
	DELETE
} walk_operation;
typedef enum walk_order {
	LEFT,
	RIGHT
} walk_order;

/* Walk the logic-adjacent text in document order */
static dom_exception walk_logic_adjacent_text_in_order(
		dom_node_internal *node, walk_operation opt,
		walk_order order, dom_string **ret, bool *cont);
/* Walk the logic-adjacent text */
static dom_exception walk_logic_adjacent_text(dom_text *text, 
		walk_operation opt, dom_string **ret);
	
/*----------------------------------------------------------------------*/
/* Constructor and Destructor */

/**
 * Create a text node
 *
 * \param doc     The owning document
 * \param name    The name of the node to create
 * \param value   The text content of the node
 * \param result  Pointer to location to receive created node
 * \return DOM_NO_ERR                on success,
 *         DOM_NO_MEM_ERR            on memory exhaustion.
 *
 * ::doc, ::name and ::value will have their reference counts increased.
 *
 * The returned node will already be referenced.
 */
dom_exception _dom_text_create(dom_document *doc,
		dom_string *name, dom_string *value,
		dom_text **result)
{
	dom_text *t;
	dom_exception err;

	/* Allocate the text node */
	t = malloc(sizeof(dom_text));
	if (t == NULL)
		return DOM_NO_MEM_ERR;

	/* And initialise the node */
	err = _dom_text_initialise(t, doc, DOM_TEXT_NODE, name, value);
	if (err != DOM_NO_ERR) {
		free(t);
		return err;
	}

	/* Compose the vtable */
	((dom_node *) t)->vtable = &text_vtable;
	((dom_node_internal *) t)->vtable = &text_protect_vtable;

	*result = t;

	return DOM_NO_ERR;
}

/**
 * Destroy a text node
 *
 * \param doc   The owning document
 * \param text  The text node to destroy
 *
 * The contents of ::text will be destroyed and ::text will be freed.
 */
void _dom_text_destroy(dom_text *text)
{
	/* Finalise node */
	_dom_text_finalise(text);

	/* Free node */
	free(text);
}

/**
 * Initialise a text node
 *
 * \param text   The node to initialise
 * \param doc    The owning document
 * \param type   The type of the node
 * \param name   The name of the node to create
 * \param value  The text content of the node
 * \return DOM_NO_ERR on success.
 *
 * ::doc, ::name and ::value will have their reference counts increased.
 */
dom_exception _dom_text_initialise(dom_text *text,
		dom_document *doc, dom_node_type type,
		dom_string *name, dom_string *value)
{
	dom_exception err;

	/* Initialise the base class */
	err = _dom_characterdata_initialise(&text->base, doc, type,
			name, value);
	if (err != DOM_NO_ERR)
		return err;

	/* Perform our type-specific initialisation */
	text->element_content_whitespace = false;

	return DOM_NO_ERR;
}

/**
 * Finalise a text node
 *
 * \param doc   The owning document
 * \param text  The text node to finalise
 *
 * The contents of ::text will be cleaned up. ::text will not be freed.
 */
void _dom_text_finalise(dom_text *text)
{
	_dom_characterdata_finalise(&text->base);
}

/*----------------------------------------------------------------------*/
/* The public virtual functions */

/**
 * Split a text node at a given character offset
 *
 * \param text  The node to split
 * \param offset  Character offset to split at
 * \param result  Pointer to location to receive new node
 * \return DOM_NO_ERR                      on success,
 *         DOM_INDEX_SIZE_ERR              if ::offset is greater than the
 *                                         number of characters in ::text,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::text is readonly.
 *
 * The returned node will be referenced. The client should unref the node
 * once it has finished with it.
 */
dom_exception _dom_text_split_text(dom_text *text,
		uint32_t offset, dom_text **result)
{
	dom_node_internal *t = (dom_node_internal *) text;
	dom_string *value;
	dom_text *res;
	uint32_t len;
	dom_exception err;

	if (_dom_node_readonly(t)) {
		return DOM_NO_MODIFICATION_ALLOWED_ERR;
	}

	err = dom_characterdata_get_length(&text->base, &len);
	if (err != DOM_NO_ERR) {
		return err;
	}

	if (offset >= len) {
		return DOM_INDEX_SIZE_ERR;
	}

	/* Retrieve the data after the split point -- 
	 * this will be the new node's value. */
	err = dom_characterdata_substring_data(&text->base, offset, 
			len - offset, &value);
	if (err != DOM_NO_ERR) {
		return err;
	}

	/* Create new node */
	err = _dom_text_create(t->owner, t->name, value, &res);
	if (err != DOM_NO_ERR) {
		dom_string_unref(value);
		return err;
	}

	/* Release our reference on the value (the new node has its own) */
	dom_string_unref(value);

	/* Delete the data after the split point */
	err = dom_characterdata_delete_data(&text->base, offset, len - offset);
	if (err != DOM_NO_ERR) {
		dom_node_unref(res);
		return err;
	}

	*result = res;

	return DOM_NO_ERR;
}

/**
 * Determine if a text node contains element content whitespace
 *
 * \param text    The node to consider
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_text_get_is_element_content_whitespace(
		dom_text *text, bool *result)
{
	*result = text->element_content_whitespace;

	return DOM_NO_ERR;
}

/**
 * Retrieve all text in Text nodes logically adjacent to a Text node
 *
 * \param text    Text node to consider
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception _dom_text_get_whole_text(dom_text *text,
		dom_string **result)
{
	return walk_logic_adjacent_text(text, COLLECT, result);
}

/**
 * Replace the text of a Text node and all logically adjacent Text nodes
 *
 * \param text     Text node to consider
 * \param content  Replacement content
 * \param result   Pointer to location to receive Text node
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if one of the Text nodes being
 *                                         replaced is readonly.
 *
 * The returned node will be referenced. The client should unref the node
 * once it has finished with it.
 */
dom_exception _dom_text_replace_whole_text(dom_text *text,
		dom_string *content, dom_text **result)
{
	dom_exception err;
	dom_string *ret;

	err = walk_logic_adjacent_text(text, DELETE, &ret);
	if (err != DOM_NO_ERR)
		return err;
	
	err = dom_characterdata_set_data(text, content);
	if (err != DOM_NO_ERR)
		return err;
	
	*result = text;
	dom_node_ref(text);

	return DOM_NO_ERR;
}

/*-----------------------------------------------------------------------*/
/* The protected virtual functions */

/* The destroy function of this class */
void __dom_text_destroy(dom_node_internal *node)
{
	_dom_text_destroy((dom_text *) node);
}

/* The copy constructor of this class */
dom_exception _dom_text_copy(dom_node_internal *old, dom_node_internal **copy)
{
	dom_text *new_text;
	dom_exception err;

	new_text = malloc(sizeof(dom_text));
	if (new_text == NULL)
		return DOM_NO_MEM_ERR;

	err = dom_text_copy_internal(old, new_text);
	if (err != DOM_NO_ERR) {
		free(new_text);
		return err;
	}

	*copy = (dom_node_internal *) new_text;

	return DOM_NO_ERR;
}

dom_exception _dom_text_copy_internal(dom_text *old, dom_text *new)
{
	dom_exception err;

	err = dom_characterdata_copy_internal(old, new);
	if (err != DOM_NO_ERR)
		return err;

	new->element_content_whitespace = old->element_content_whitespace;

	return DOM_NO_ERR;
}

/*----------------------------------------------------------------------*/
/* Helper functions */

/**
 * Walk the logic adjacent text in certain order
 *
 * \param node   The start Text node
 * \param opt    The operation on each Text Node
 * \param order  The order
 * \param ret    The string of the logic adjacent text 
 * \param cont   Whether the logic adjacent text is interrupt here
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception walk_logic_adjacent_text_in_order(
		dom_node_internal *node, walk_operation opt,
		walk_order order, dom_string **ret, bool *cont)
{
	dom_exception err;
	dom_string *data, *tmp;
	dom_node_internal *parent;

	/* If we reach the leaf of the DOM tree, just return to continue
	 * to next sibling of our parent */
	if (node == NULL) {
		*cont = true;
		return DOM_NO_ERR;
	}

	parent = dom_node_get_parent(node);

	while (node != NULL) {
		dom_node_internal *p;

		/* If we reach the boundary of logical-adjacent text, we stop */
		if (node->type == DOM_ELEMENT_NODE || 
				node->type == DOM_COMMENT_NODE ||
				node->type == 
				DOM_PROCESSING_INSTRUCTION_NODE) {
			*cont = false;
			return DOM_NO_ERR;
		}

		if (node->type == DOM_TEXT_NODE) {
			/* According the DOM spec, text node never have child */
			assert(node->first_child == NULL);
			assert(node->last_child == NULL);
			if (opt == COLLECT) {
				err = dom_characterdata_get_data(node, &data);
				if (err == DOM_NO_ERR)
					return err;

				tmp = *ret;
				if (order == LEFT) {
					err = dom_string_concat(data, tmp, ret);
					if (err == DOM_NO_ERR)
						return err;
				} else if (order == RIGHT) {
					err = dom_string_concat(tmp, data, ret);
					if (err == DOM_NO_ERR)
						return err;
				}

				dom_string_unref(tmp);
				dom_string_unref(data);

				*cont = true;
				return DOM_NO_ERR;
			}

			if (opt == DELETE) {
				dom_node_internal *tn;
				err = dom_node_remove_child(node->parent,
						node, (void *) &tn);
				if (err != DOM_NO_ERR)
					return err;

				*cont = true;
				dom_node_unref(tn);
				return DOM_NO_ERR;
			}
		}

		p = dom_node_get_parent(node);
		if (order == LEFT) {
			if (node->last_child != NULL) {
				node = node->last_child;
			} else if (node->previous != NULL) {
				node = node->previous;
			} else {
				while (p != parent && node == p->last_child) {
					node = p;
					p = dom_node_get_parent(p);
				}

				node = node->previous;
			}
		} else {
			if (node->first_child != NULL) {
				node = node->first_child;
			} else if (node->next != NULL) {
				node = node->next;
			} else {
				while (p != parent && node == p->first_child) {
					node = p;
					p = dom_node_get_parent(p);
				}

				node = node->next;
			}
		}
	}

	return DOM_NO_ERR;
}

/**
 * Traverse the logic adjacent text.
 *
 * \param text  The Text Node from which we start traversal
 * \param opt   The operation code
 * \param ret   The returned string if the opt is COLLECT
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception walk_logic_adjacent_text(dom_text *text, 
		walk_operation opt, dom_string **ret)
{
	dom_node_internal *node = (dom_node_internal *) text;
	dom_node_internal *parent = node->parent;
	dom_node_internal *left = node->previous;
	dom_node_internal *right = node->next;
	dom_exception err;
	bool cont;
	
	if (parent->type == DOM_ENTITY_NODE) {
		return DOM_NOT_SUPPORTED_ERR;
	}

	*ret = NULL;

	/* Firstly, we look our left */
	err = walk_logic_adjacent_text_in_order(left, opt, LEFT, ret, &cont);
	if (err != DOM_NO_ERR) {
		if (opt == COLLECT) {
			dom_string_unref(*ret);
			*ret = NULL;
		}
		return err;
	}

	/* Ourself */
	if (opt == COLLECT) {
		dom_string *data = NULL, *tmp = NULL;
		err = dom_characterdata_get_data(text, &data);
		if (err != DOM_NO_ERR) {
			dom_string_unref(*ret);
			return err;
		}

		if (*ret != NULL) {
			err = dom_string_concat(*ret, data, &tmp);
			dom_string_unref(data);
			dom_string_unref(*ret);
			if (err != DOM_NO_ERR) {
				return err;
			}

			*ret = tmp;
		} else {
			*ret = data;
		}
	} else {
			dom_node_internal *tn;
			err = dom_node_remove_child(node->parent, node,
					(void *) &tn);
			if (err != DOM_NO_ERR)
				return err;
			dom_node_unref(tn);
	}

	/* Now, look right */
	err = walk_logic_adjacent_text_in_order(right, opt, RIGHT, ret, &cont);
	if (err != DOM_NO_ERR) {
		if (opt == COLLECT) {
			dom_string_unref(*ret);
			*ret = NULL;
		}
		return err;
	}

	return DOM_NO_ERR;
}

