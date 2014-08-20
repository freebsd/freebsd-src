/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdlib.h>

#include "core/characterdata.h"
#include "core/comment.h"
#include "core/document.h"

#include "utils/utils.h"

/**
 * A DOM Comment node
 */
struct dom_comment {
	dom_characterdata base;	/**< Base node */
};

static struct dom_node_protect_vtable comment_protect_vtable = {
	DOM_COMMENT_PROTECT_VTABLE
};

/**
 * Create a comment node
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
dom_exception _dom_comment_create(dom_document *doc,
		dom_string *name, dom_string *value,
		dom_comment **result)
{
	dom_comment *c;
	dom_exception err;

	/* Allocate the comment node */
	c = malloc(sizeof(dom_comment));
	if (c == NULL)
		return DOM_NO_MEM_ERR;

	/* Set the virtual table */
	((dom_node_internal *) c)->base.vtable = &characterdata_vtable;
	((dom_node_internal *) c)->vtable = &comment_protect_vtable;

	/* And initialise the node */
	err = _dom_characterdata_initialise(&c->base, doc, DOM_COMMENT_NODE,
			name, value);
	if (err != DOM_NO_ERR) {
		free(c);
		return err;
	}

	*result = c;

	return DOM_NO_ERR;
}

/**
 * Destroy a comment node
 *
 * \param comment  The node to destroy
 *
 * The contents of ::comment will be destroyed and ::comment will be freed
 */
void _dom_comment_destroy(dom_comment *comment)
{
	/* Finalise base class contents */
	_dom_characterdata_finalise(&comment->base);

	/* Free node */
	free(comment);
}


/*-----------------------------------------------------------------------*/
/* The protected virtual functions */

/* The virtual destroy function */
void __dom_comment_destroy(dom_node_internal *node)
{
	_dom_comment_destroy((dom_comment *) node);
}

/* The copy constructor of this class */
dom_exception _dom_comment_copy(dom_node_internal *old, 
		dom_node_internal **copy)
{
	dom_comment *new_comment;
	dom_exception err;

	new_comment = malloc(sizeof(dom_comment));
	if (new_comment == NULL)
		return DOM_NO_MEM_ERR;

	err = dom_characterdata_copy_internal(old, new_comment);
	if (err != DOM_NO_ERR) {
		free(new_comment);
		return err;
	}

	*copy = (dom_node_internal *) new_comment;

	return DOM_NO_ERR;
}

