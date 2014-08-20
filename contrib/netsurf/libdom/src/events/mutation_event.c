/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdlib.h>

#include "events/mutation_event.h"
#include "core/document.h"

static void _virtual_dom_mutation_event_destroy(struct dom_event *evt);

static struct dom_event_private_vtable _event_vtable = {
	_virtual_dom_mutation_event_destroy
};

/* Constructor */
dom_exception _dom_mutation_event_create(struct dom_document *doc, 
		struct dom_mutation_event **evt)
{
	*evt = malloc(sizeof(dom_mutation_event));
	if (*evt == NULL) 
		return DOM_NO_MEM_ERR;
	
	((struct dom_event *) *evt)->vtable = &_event_vtable;

	return _dom_mutation_event_initialise(doc, *evt);
}

/* Destructor */
void _dom_mutation_event_destroy(struct dom_mutation_event *evt)
{
	_dom_mutation_event_finalise(evt);

	free(evt);
}

/* Initialise function */
dom_exception _dom_mutation_event_initialise(struct dom_document *doc, 
		struct dom_mutation_event *evt)
{
	evt->related_node = NULL;
	evt->prev_value = NULL;
	evt->new_value = NULL;
	evt->attr_name = NULL;

	return _dom_event_initialise(doc, &evt->base);
}

/* Finalise function */
void _dom_mutation_event_finalise(struct dom_mutation_event *evt)
{
	dom_node_unref(evt->related_node);
	dom_string_unref(evt->prev_value);
	dom_string_unref(evt->new_value);
	dom_string_unref(evt->attr_name);
	
	evt->related_node = NULL;
	evt->prev_value = NULL;
	evt->new_value = NULL;
	evt->attr_name = NULL;

	_dom_event_finalise(&evt->base);
}

/* The virtual destroy function */
void _virtual_dom_mutation_event_destroy(struct dom_event *evt)
{
	_dom_mutation_event_destroy((dom_mutation_event *) evt);
}

/*----------------------------------------------------------------------*/
/* The public API */

/**
 * Get the related node
 *
 * \param evt   The Event object
 * \param node  The related node
 * \return DOM_NO_ERR.
 */
dom_exception _dom_mutation_event_get_related_node(dom_mutation_event *evt,
		struct dom_node **node)
{
	*node = evt->related_node;
	dom_node_ref(*node);

	return DOM_NO_ERR;
}

/** 
 * Get the old value
 *
 * \param evt  The Event object
 * \param ret  The old value
 * \return DOM_NO_ERR.
 */
dom_exception _dom_mutation_event_get_prev_value(dom_mutation_event *evt,
		dom_string **ret)
{
	*ret = evt->prev_value;
	dom_string_ref(*ret);

	return DOM_NO_ERR;
}

/**
 * Get the new value
 *
 * \param evt  The Event object
 * \param ret  The new value
 * \return DOM_NO_ERR.
 */
dom_exception _dom_mutation_event_get_new_value(dom_mutation_event *evt,
		dom_string **ret)
{
	*ret = evt->new_value;
	dom_string_ref(*ret);

	return DOM_NO_ERR;
}

/**
 * Get the attr name
 *
 * \param evt  The Event object
 * \param ret  The attribute name
 * \return DOM_NO_ERR.
 */
dom_exception _dom_mutation_event_get_attr_name(dom_mutation_event *evt,
		dom_string **ret)
{
	*ret = evt->attr_name;
	dom_string_ref(*ret);

	return DOM_NO_ERR;
}

/**
 * Get the way the attribute change
 *
 * \param evt   The Event object
 * \param type  The change type
 * \return DOM_NO_ERR.
 */
dom_exception _dom_mutation_event_get_attr_change(dom_mutation_event *evt,
		dom_mutation_type *type)
{
	*type = evt->change;

	return DOM_NO_ERR;
}

/**
 * Initialise the MutationEvent
 *
 * \param evt         The Event object
 * \param type        The type of this UIEvent
 * \param bubble      Whether this event can bubble
 * \param cancelable  Whether this event is cancelable
 * \param node        The mutation node
 * \param prev_value  The old value
 * \param new_value   The new value
 * \param attr_name   The attribute's name
 * \param change      The change type
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_mutation_event_init(dom_mutation_event *evt, 
		dom_string *type, bool bubble, bool cancelable, 
		struct dom_node *node, dom_string *prev_value, 
		dom_string *new_value, dom_string *attr_name,
		dom_mutation_type change)
{
	evt->related_node = node;
	dom_node_ref(node);

	evt->prev_value = prev_value;
	dom_string_ref(prev_value);

	evt->new_value = new_value;
	dom_string_ref(new_value);

	evt->attr_name = attr_name;
	dom_string_ref(attr_name);

	evt->change = change;

	return _dom_event_init(&evt->base, type, bubble, cancelable);
}

/**
 * Initialise the MutationEvent with namespace
 *
 * \param evt         The Event object
 * \param namespace   The namespace
 * \param type        The type of this UIEvent
 * \param bubble      Whether this event can bubble
 * \param cancelable  Whether this event is cancelable
 * \param node        The mutation node
 * \param prev_value  The old value
 * \param new_value   The new value
 * \param attr_name   The attribute's name
 * \param change      The change type
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_mutation_event_init_ns(dom_mutation_event *evt,
		dom_string *namespace, dom_string *type,
		bool bubble, bool cancelable, struct dom_node *node,
		dom_string *prev_value, dom_string *new_value,
		dom_string *attr_name, dom_mutation_type change)
{
	evt->related_node = node;
	dom_node_ref(node);

	evt->prev_value = prev_value;
	dom_string_ref(prev_value);

	evt->new_value = new_value;
	dom_string_ref(new_value);

	evt->attr_name = attr_name;
	dom_string_ref(attr_name);

	evt->change = change;

	return _dom_event_init_ns(&evt->base, namespace, type, bubble,
			cancelable);
}

