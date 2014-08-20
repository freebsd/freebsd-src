/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include "events/event.h"

#include "core/string.h"
#include "core/node.h"
#include "core/document.h"
#include "utils/utils.h"

static void _virtual_dom_event_destroy(dom_event *evt);

static struct dom_event_private_vtable _event_vtable = {
	_virtual_dom_event_destroy
};

/* Constructor */
dom_exception _dom_event_create(dom_document *doc, dom_event **evt)
{
	*evt = (dom_event *) malloc(sizeof(dom_event));
	if (*evt == NULL) 
		return DOM_NO_MEM_ERR;
	
	(*evt)->vtable = &_event_vtable;

	return _dom_event_initialise(doc, *evt);
}

/* Destructor */
void _dom_event_destroy(dom_event *evt)
{
	_dom_event_finalise(evt);

	free(evt);
}

/* Initialise function */
dom_exception _dom_event_initialise(dom_document *doc, dom_event *evt)
{
	assert(doc != NULL);

	evt->doc = doc;
	evt->stop = false;
	evt->stop_now = false;
	evt->prevent_default = false;
	evt->custom = false;

	evt->type = NULL;

	evt->namespace = NULL;

	evt->refcnt = 1;
	evt->in_dispatch = false;

	return DOM_NO_ERR;
}

/* Finalise function */
void _dom_event_finalise(dom_event *evt)
{
	if (evt->type != NULL)
		dom_string_unref(evt->type);
	if (evt->namespace != NULL)
		dom_string_unref(evt->namespace);
	
	evt->stop = false;
	evt->stop_now = false;
	evt->prevent_default = false;
	evt->custom = false;

	evt->type = NULL;

	evt->namespace = NULL;

	evt->in_dispatch = false;
}

/* The virtual destroy function */
void _virtual_dom_event_destroy(dom_event *evt)
{
	_dom_event_destroy(evt);
}

/*----------------------------------------------------------------------*/
/* The public API */

/**
 * Claim a reference on this event object
 *
 * \param evt  The Event object
 */
void _dom_event_ref(dom_event *evt)
{
	evt->refcnt++;
}

/**
 * Release a reference on this event object
 *
 * \param evt  The Event object
 */
void _dom_event_unref(dom_event *evt)
{
	if (evt->refcnt > 0)
		evt->refcnt--;

	if (evt->refcnt == 0)
		dom_event_destroy(evt);
}


/**
 * Get the event type
 *
 * \param evt   The event object
 * \parnm type  The returned event type
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_event_get_type(dom_event *evt, dom_string **type)
{
	*type = dom_string_ref(evt->type);
	
	return DOM_NO_ERR;
}

/**
 * Get the target node of this event
 *
 * \param evt     The event object
 * \param target  The target node
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_event_get_target(dom_event *evt, dom_event_target **target)
{
	*target = evt->target;
	dom_node_ref(*target);

	return DOM_NO_ERR;
}

/**
 * Get the current target of this event
 *
 * \param evt      The event object
 * \param current  The current event target node
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_event_get_current_target(dom_event *evt,
		dom_event_target **current)
{
	*current = evt->current;
	dom_node_ref(*current);

	return DOM_NO_ERR;
}

/**
 * Get whether this event can bubble
 *
 * \param evt      The Event object
 * \param bubbles  The returned value
 * \return DOM_NO_ERR.
 */
dom_exception _dom_event_get_bubbles(dom_event *evt, bool *bubbles)
{
	*bubbles = evt->bubble;
	return DOM_NO_ERR;
}

/**
 * Get whether this event can be cancelable
 *
 * \param evt         The Event object
 * \param cancelable  The returned value
 * \return DOM_NO_ERR.
 */
dom_exception _dom_event_get_cancelable(dom_event *evt, bool *cancelable)
{
	*cancelable = evt->cancelable;
	return DOM_NO_ERR;
}

/**
 * Get the event's generation timestamp 
 *
 * \param evt        The Event object
 * \param timestamp  The returned value
 * \return DOM_NO_ERR.
 */
dom_exception _dom_event_get_timestamp(dom_event *evt, unsigned int *timestamp)
{
	*timestamp = evt->timestamp;
	return DOM_NO_ERR;
}

/**
 * Stop propagation of the event
 *
 * \param evt  The Event object
 * \return DOM_NO_ERR.
 */
dom_exception _dom_event_stop_propagation(dom_event *evt)
{
	evt->stop = true;

	return DOM_NO_ERR;
}

/**
 * Prevent the default action of this event
 *
 * \param evt  The Event object
 * \return DOM_NO_ERR.
 */
dom_exception _dom_event_prevent_default(dom_event *evt)
{
	evt->prevent_default = true;
	return DOM_NO_ERR;
}

/**
 * Initialise the event object 
 *
 * \param evt         The event object
 * \param type        The type of this event
 * \param bubble      Whether this event can bubble
 * \param cancelable  Whether this event is cancelable
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_event_init(dom_event *evt, dom_string *type, 
		bool bubble, bool cancelable)
{
	assert(evt->doc != NULL);

	evt->type = dom_string_ref(type);
	evt->bubble = bubble;
	evt->cancelable = cancelable;

	evt->timestamp = time(NULL);

	return DOM_NO_ERR;
}

/**
 * Get the namespace of this event
 *
 * \param evt        The event object
 * \param namespace  The returned namespace of this event
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_event_get_namespace(dom_event *evt,
		dom_string **namespace)
{
	*namespace = dom_string_ref(evt->namespace);
	
	return DOM_NO_ERR;
}

/**
 * Whether this is a custom event
 *
 * \param evt     The event object
 * \param custom  The returned value
 * \return DOM_NO_ERR.
 */
dom_exception _dom_event_is_custom(dom_event *evt, bool *custom)
{
	*custom = evt->custom;

	return DOM_NO_ERR;
}
	
/**
 * Stop the event propagation immediately
 *
 * \param evt  The event object
 * \return DOM_NO_ERR.
 */
dom_exception _dom_event_stop_immediate_propagation(dom_event *evt)
{
	evt->stop_now = true;

	return DOM_NO_ERR;
}

/**
 * Whether the default action is prevented
 *
 * \param evt        The event object
 * \param prevented  The returned value
 * \return DOM_NO_ERR.
 */
dom_exception _dom_event_is_default_prevented(dom_event *evt, bool *prevented)
{
	*prevented = evt->prevent_default;

	return DOM_NO_ERR;
}

/**
 * Initialise the event with namespace
 *
 * \param evt         The event object
 * \param namespace   The namespace of this event
 * \param type        The event type
 * \param bubble      Whether this event has a bubble phase
 * \param cancelable  Whether this event is cancelable
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_event_init_ns(dom_event *evt, dom_string *namespace,
		dom_string *type, bool bubble, bool cancelable)
{
	assert(evt->doc != NULL);

	evt->type = dom_string_ref(type);

	evt->namespace = dom_string_ref(namespace);

	evt->bubble = bubble;
	evt->cancelable = cancelable;

	return DOM_NO_ERR;
}

