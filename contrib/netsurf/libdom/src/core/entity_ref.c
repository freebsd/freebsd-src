/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdlib.h>

#include "core/document.h"
#include "core/entity_ref.h"
#include "core/node.h"
#include "utils/utils.h"

/**
 * A DOM entity reference
 */
struct dom_entity_reference {
	dom_node_internal base;		/**< Base node */
};

static struct dom_node_vtable er_vtable = {
	{
		DOM_NODE_EVENT_TARGET_VTABLE
	},
	DOM_NODE_VTABLE
};

static struct dom_node_protect_vtable er_protect_vtable = {
	DOM_ER_PROTECT_VTABLE
};

/**
 * Create an entity reference
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
dom_exception _dom_entity_reference_create(dom_document *doc,
		dom_string *name, dom_string *value,
		dom_entity_reference **result)
{
	dom_entity_reference *e;
	dom_exception err;

	/* Allocate the comment node */
	e = malloc(sizeof(dom_entity_reference));
	if (e == NULL)
		return DOM_NO_MEM_ERR;

	e->base.base.vtable = &er_vtable;
	e->base.vtable = &er_protect_vtable;

	/* And initialise the node */
	err = _dom_entity_reference_initialise(&e->base, doc, 
			DOM_ENTITY_REFERENCE_NODE, name, value, NULL, NULL);
	if (err != DOM_NO_ERR) {
		free(e);
		return err;
	}

	*result = e;

	return DOM_NO_ERR;
}

/**
 * Destroy an entity reference
 *
 * \param entity  The entity reference to destroy
 *
 * The contents of ::entity will be destroyed and ::entity will be freed.
 */
void _dom_entity_reference_destroy(dom_entity_reference *entity)
{
	/* Finalise base class */
	_dom_entity_reference_finalise(&entity->base);

	/* Destroy fragment */
	free(entity);
}

/**
 * Get the textual representation of an EntityRererence
 *
 * \param entity  The entity reference to get the textual representation of
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR on success.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unrer the string once it has
 * finished with it.
 */
dom_exception _dom_entity_reference_get_textual_representation(
		dom_entity_reference *entity, dom_string **result)
{
	UNUSED(entity);
	UNUSED(result);

	return DOM_NOT_SUPPORTED_ERR;
}

/*-----------------------------------------------------------------------*/

/* Following comes the protected vtable  */

/* The virtual destroy function of this class */
void _dom_er_destroy(dom_node_internal *node)
{
	_dom_entity_reference_destroy((dom_entity_reference *) node);
}

/* The copy constructor of this class */
dom_exception _dom_er_copy(dom_node_internal *old, dom_node_internal **copy)
{
	dom_entity_reference *new_er;
	dom_exception err;

	new_er = malloc(sizeof(dom_entity_reference));
	if (new_er == NULL)
		return DOM_NO_MEM_ERR;

	err = dom_node_copy_internal(old, new_er);
	if (err != DOM_NO_ERR) {
		free(new_er);
		return err;
	}

	*copy = (dom_node_internal *) new_er;

	return DOM_NO_ERR;
}

