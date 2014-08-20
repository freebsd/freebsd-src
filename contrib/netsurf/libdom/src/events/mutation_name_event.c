/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdlib.h>

#include "events/mutation_name_event.h"
#include "core/document.h"

#include "utils/utils.h"

static void _virtual_dom_mutation_name_event_destroy(struct dom_event *evt);

static struct dom_event_private_vtable _event_vtable = {
	_virtual_dom_mutation_name_event_destroy
};

/* Constructor */
dom_exception _dom_mutation_name_event_create(struct dom_document *doc, 
		struct dom_mutation_name_event **evt)
{
	*evt = malloc(sizeof(dom_mutation_name_event));
	if (*evt == NULL) 
		return DOM_NO_MEM_ERR;
	
	((struct dom_event *) *evt)->vtable = &_event_vtable;

	return _dom_mutation_name_event_initialise(doc, *evt);
}

/* Destructor */
void _dom_mutation_name_event_destroy(struct dom_mutation_name_event *evt)
{
	_dom_mutation_name_event_finalise(evt);

	free(evt);
}

/* Initialise function */
dom_exception _dom_mutation_name_event_initialise(struct dom_document *doc, 
		struct dom_mutation_name_event *evt)
{
	evt->prev_namespace = NULL;
	evt->prev_nodename = NULL;

	return _dom_event_initialise(doc, (dom_event *) evt);
}

/* Finalise function */
void _dom_mutation_name_event_finalise(struct dom_mutation_name_event *evt)
{
	dom_string_unref(evt->prev_namespace);
	dom_string_unref(evt->prev_nodename);

	_dom_event_finalise((dom_event *) evt);
}

/* The virtual destroy function */
void _virtual_dom_mutation_name_event_destroy(struct dom_event *evt)
{
	_dom_mutation_name_event_destroy((dom_mutation_name_event *) evt);
}

/*----------------------------------------------------------------------*/
/* The public API */

/**
 * Get the previous namespace
 *
 * \param evt        The Event object
 * \param namespace  The previous namespace of this event
 * \return DOM_NO_ERR.
 */
dom_exception _dom_mutation_name_event_get_prev_namespace(
		dom_mutation_name_event *evt, dom_string **namespace)
{
	*namespace = evt->prev_namespace;
	dom_string_ref(*namespace);

	return DOM_NO_ERR;
}

/**
 * Get the previous node name
 *
 * \param evt   The Event object
 * \param name  The previous node name
 * \return DOM_NO_ERR.
 */
dom_exception _dom_mutation_name_event_get_prev_node_name(
		dom_mutation_name_event *evt, dom_string **name)
{
	*name = evt->prev_nodename;
	dom_string_ref(*name);

	return DOM_NO_ERR;
}

/**
 * Initialise the MutationNameEvent
 *
 * \param evt         The Event object
 * \param type        The type of this UIEvent
 * \param bubble      Whether this event can bubble
 * \param cancelable  Whether this event is cancelable
 * \param node        The node whose name change
 * \param prev_ns     The old namespace
 * \param prev_name   The old name
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_mutation_name_event_init(dom_mutation_name_event *evt, 
		dom_string *type, bool bubble, bool cancelable, 
		struct dom_node *node, dom_string *prev_ns, 
		dom_string *prev_name)
{
	evt->prev_namespace = prev_ns;
	dom_string_ref(prev_ns);

	evt->prev_nodename = prev_name;
	dom_string_ref(prev_name);

	return _dom_mutation_event_init((dom_mutation_event *) evt, type,
			bubble, cancelable, node, NULL, NULL, NULL,
			DOM_MUTATION_MODIFICATION);
}

/**
 * Initialise the MutationNameEvent with namespace
 *
 * \param evt         The Event object
 * \param namespace   The namespace
 * \param type        The type of this UIEvent
 * \param bubble      Whether this event can bubble
 * \param cancelable  Whether this event is cancelable
 * \param node        The node whose name change
 * \param prev_ns     The old namespace
 * \param prev_name   The old name
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_mutation_name_event_init_ns(dom_mutation_name_event *evt, 
		dom_string *namespace, dom_string *type,
		bool bubble, bool cancelable, struct dom_node *node,
		dom_string *prev_ns, dom_string *prev_name)
{
	evt->prev_namespace = prev_ns;
	dom_string_ref(prev_ns);

	evt->prev_nodename = prev_name;
	dom_string_ref(prev_name);

	return _dom_mutation_event_init_ns((dom_mutation_event *) evt,
			namespace, type, bubble, cancelable, node, NULL,
			NULL, NULL, DOM_MUTATION_MODIFICATION);
}

