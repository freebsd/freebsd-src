/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdlib.h>

#include "events/custom_event.h"

#include "core/document.h"

static void _virtual_dom_custom_event_destroy(struct dom_event *evt);

static struct dom_event_private_vtable _event_vtable = {
	_virtual_dom_custom_event_destroy
};

/* Constructor */
dom_exception _dom_custom_event_create(struct dom_document *doc, 
		struct dom_custom_event **evt)
{
	*evt = malloc(sizeof(dom_custom_event));
	if (*evt == NULL) 
		return DOM_NO_MEM_ERR;
	
	((struct dom_event *) *evt)->vtable = &_event_vtable;

	return _dom_custom_event_initialise(doc, *evt);
}

/* Destructor */
void _dom_custom_event_destroy(struct dom_custom_event *evt)
{
	_dom_custom_event_finalise(evt);

	free(evt);
}

/* Initialise function */
dom_exception _dom_custom_event_initialise(struct dom_document *doc, 
		struct dom_custom_event *evt)
{
	evt->detail = NULL;
	return _dom_event_initialise(doc, &evt->base);
}

/* Finalise function */
void _dom_custom_event_finalise(struct dom_custom_event *evt)
{
	evt->detail = NULL;
	_dom_event_finalise(&evt->base);
}

/* The virtual destroy function */
void _virtual_dom_custom_event_destroy(struct dom_event *evt)
{
	_dom_custom_event_destroy((dom_custom_event *) evt);
}

/*----------------------------------------------------------------------*/
/* The public API */

/**
 * Get the detail object of this custom event
 *
 * \param evt     The Event object
 * \param detail  The returned detail object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_custom_event_get_detail(dom_custom_event *evt,
		void **detail)
{
	*detail = evt->detail;

	return DOM_NO_ERR;
}

/**
 * Initialise this custom event
 *
 * \param evt         The Event object
 * \param namespace   The namespace of this new Event
 * \param type        The Event type
 * \param bubble      Whether this event can bubble
 * \param cancelable  Whether this event is cancelable
 * \param detail      The detail object of this custom event
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_custom_event_init_ns(dom_custom_event *evt, 
		dom_string *namespace, dom_string *type,
		bool bubble, bool cancelable, void *detail)
{
	evt->detail = detail;
	return _dom_event_init_ns(&evt->base, namespace, type, bubble, 
			cancelable);
}

