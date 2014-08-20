/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stdlib.h>

#include "core/document.h"
#include "core/node.h"
#include "core/pi.h"

#include "utils/utils.h"

/**
 * A DOM processing instruction
 */
struct dom_processing_instruction {
	dom_node_internal base;		/**< Base node */
};

static struct dom_node_vtable pi_vtable = {
	{
		DOM_NODE_EVENT_TARGET_VTABLE
	},
	DOM_NODE_VTABLE
};

static struct dom_node_protect_vtable pi_protect_vtable = {
	DOM_PI_PROTECT_VTABLE
};
/**
 * Create a processing instruction
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
dom_exception _dom_processing_instruction_create(dom_document *doc,
		dom_string *name, dom_string *value,
		dom_processing_instruction **result)
{
	dom_processing_instruction *p;
	dom_exception err;

	/* Allocate the comment node */
	p = malloc(sizeof(dom_processing_instruction));
	if (p == NULL)
		return DOM_NO_MEM_ERR;
	
	p->base.base.vtable = &pi_vtable;
	p->base.vtable = &pi_protect_vtable;

	/* And initialise the node */
	err = _dom_processing_instruction_initialise(&p->base, doc,
			DOM_PROCESSING_INSTRUCTION_NODE,
			name, value, NULL, NULL);
	if (err != DOM_NO_ERR) {
		free(p);
		return err;
	}

	*result = p;

	return DOM_NO_ERR;
}

/**
 * Destroy a processing instruction
 *
 * \param pi   The processing instruction to destroy
 *
 * The contents of ::pi will be destroyed and ::pi will be freed.
 */
void _dom_processing_instruction_destroy(dom_processing_instruction *pi)
{
	/* Finalise base class */
	_dom_processing_instruction_finalise(&pi->base);

	/* Free processing instruction */
	free(pi);
}

/*-----------------------------------------------------------------------*/

/* Following comes the protected vtable  */

/* The virtual destroy function of this class */
void _dom_pi_destroy(dom_node_internal *node)
{
	_dom_processing_instruction_destroy(
			(dom_processing_instruction *) node);
}

/* The copy constructor of this class */
dom_exception _dom_pi_copy(dom_node_internal *old, dom_node_internal **copy)
{
	dom_processing_instruction *new_pi;
	dom_exception err;

	new_pi = malloc(sizeof(dom_processing_instruction));
	if (new_pi == NULL)
		return DOM_NO_MEM_ERR;

	err = dom_node_copy_internal(old, new_pi);
	if (err != DOM_NO_ERR) {
		free(new_pi);
		return err;
	}

	*copy = (dom_node_internal *) copy;

	return DOM_NO_ERR;
}

