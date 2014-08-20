/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com> 
 */

#include <stdlib.h>

#include "core/cdatasection.h"
#include "core/document.h"
#include "core/text.h"
#include "utils/utils.h"

/**
 * A DOM CDATA section
 */
struct dom_cdata_section {
	dom_text base;		/**< Base node */
};

static struct dom_node_protect_vtable cdata_section_protect_vtable = {
	DOM_CDATA_SECTION_PROTECT_VTABLE
};

/**
 * Create a CDATA section
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
dom_exception _dom_cdata_section_create(dom_document *doc,
		dom_string *name, dom_string *value,
		dom_cdata_section **result)
{
	dom_cdata_section *c;
	dom_exception err;

	/* Allocate the comment node */
	c = malloc(sizeof(dom_cdata_section));
	if (c == NULL)
		return DOM_NO_MEM_ERR;
	
	/* Set up vtable */
	((dom_node_internal *) c)->base.vtable = &text_vtable;
	((dom_node_internal *) c)->vtable = &cdata_section_protect_vtable;

	/* And initialise the node */
	err = _dom_cdata_section_initialise(&c->base, doc,
			DOM_CDATA_SECTION_NODE, name, value);
	if (err != DOM_NO_ERR) {
		free(c);
		return err;
	}

	*result = c;

	return DOM_NO_ERR;
}

/**
 * Destroy a CDATA section
 *
 * \param cdata  The cdata section to destroy
 *
 * The contents of ::cdata will be destroyed and ::cdata will be freed.
 */
void _dom_cdata_section_destroy(dom_cdata_section *cdata)
{
	/* Clean up base node contents */
	_dom_cdata_section_finalise(&cdata->base);

	/* Destroy the node */
	free(cdata);
}

/*--------------------------------------------------------------------------*/

/* The protected virtual functions */

/* The virtual destroy function of this class */
void __dom_cdata_section_destroy(dom_node_internal *node)
{
	_dom_cdata_section_destroy((dom_cdata_section *) node);
}

/* The copy constructor of this class */
dom_exception _dom_cdata_section_copy(dom_node_internal *old, 
		dom_node_internal **copy)
{
	dom_cdata_section *new_cdata;
	dom_exception err;

	new_cdata = malloc(sizeof(dom_cdata_section));
	if (new_cdata == NULL)
		return DOM_NO_MEM_ERR;

	err = dom_text_copy_internal(old, new_cdata);
	if (err != DOM_NO_ERR) {
		free(new_cdata);
		return err;
	}

	*copy = (dom_node_internal *) new_cdata;

	return DOM_NO_ERR;	
}

