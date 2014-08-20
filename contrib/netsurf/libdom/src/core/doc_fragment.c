/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdlib.h>

#include <dom/core/node.h>

#include "core/document.h"
#include "core/doc_fragment.h"
#include "core/node.h"
#include "utils/utils.h"

/**
 * A DOM document fragment
 */
struct dom_document_fragment {
	dom_node_internal base;		/**< Base node */
};

static struct dom_node_vtable df_vtable = {
	{
		DOM_NODE_EVENT_TARGET_VTABLE
	},
	DOM_NODE_VTABLE
};

static struct dom_node_protect_vtable df_protect_vtable = {
	DOM_DF_PROTECT_VTABLE
};

/**
 * Create a document fragment
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
dom_exception _dom_document_fragment_create(dom_document *doc,
		dom_string *name, dom_string *value,
		dom_document_fragment **result)
{
	dom_document_fragment *f;
	dom_exception err;

	f = malloc(sizeof(dom_document_fragment));
	if (f == NULL)
		return DOM_NO_MEM_ERR;

	f->base.base.vtable = &df_vtable;
	f->base.vtable = &df_protect_vtable;

	/* And initialise the node */
	err = _dom_document_fragment_initialise(&f->base, doc, 
			DOM_DOCUMENT_FRAGMENT_NODE, name, value, NULL, NULL);
	if (err != DOM_NO_ERR) {
		free(f);
		return err;
	}

	*result = f;

	return DOM_NO_ERR;
}

/**
 * Destroy a document fragment
 *
 * \param frag  The document fragment to destroy
 *
 * The contents of ::frag will be destroyed and ::frag will be freed.
 */
void _dom_document_fragment_destroy(dom_document_fragment *frag)
{
	/* Finalise base class */
	_dom_document_fragment_finalise(&frag->base);

	/* Destroy fragment */
	free(frag);
}

/*-----------------------------------------------------------------------*/

/* Overload protected functions */

/* The virtual destroy function of this class */
void _dom_df_destroy(dom_node_internal *node)
{
	_dom_document_fragment_destroy((dom_document_fragment *) node);
}

/* The copy constructor of this class */
dom_exception _dom_df_copy(dom_node_internal *old, dom_node_internal **copy)
{
	dom_document_fragment *new_f;
	dom_exception err;

	new_f = malloc(sizeof(dom_document_fragment));
	if (new_f == NULL)
		return DOM_NO_MEM_ERR;

	err = dom_node_copy_internal(old, new_f);
	if (err != DOM_NO_ERR) {
		free(new_f);
		return err;
	}

	*copy = (dom_node_internal *) new_f;

	return DOM_NO_ERR;
}

