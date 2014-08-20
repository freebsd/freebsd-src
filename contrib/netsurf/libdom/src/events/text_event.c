/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdlib.h>

#include "events/text_event.h"
#include "core/document.h"

static void _virtual_dom_text_event_destroy(struct dom_event *evt);

static struct dom_event_private_vtable _event_vtable = {
	_virtual_dom_text_event_destroy
};

/* Constructor */
dom_exception _dom_text_event_create(struct dom_document *doc, 
		struct dom_text_event **evt)
{
	*evt = malloc(sizeof(dom_text_event));
	if (*evt == NULL) 
		return DOM_NO_MEM_ERR;
	
	((struct dom_event *) *evt)->vtable = &_event_vtable;

	return _dom_text_event_initialise(doc, *evt);
}

/* Destructor */
void _dom_text_event_destroy(struct dom_text_event *evt)
{
	_dom_text_event_finalise(evt);

	free(evt);
}

/* Initialise function */
dom_exception _dom_text_event_initialise(struct dom_document *doc, 
		struct dom_text_event *evt)
{
	evt->data = NULL;
	return _dom_ui_event_initialise(doc, &evt->base);
}

/* Finalise function */
void _dom_text_event_finalise(struct dom_text_event *evt)
{
	dom_string_unref(evt->data);
	_dom_ui_event_finalise(&evt->base);
}

/* The virtual destroy function */
void _virtual_dom_text_event_destroy(struct dom_event *evt)
{
	_dom_text_event_destroy((dom_text_event *) evt);
}

/*----------------------------------------------------------------------*/
/* The public API */

/**
 * Get the internal data of this event
 *
 * \param evt   The Event object
 * \param data  The internal data of this Event
 * \return DOM_NO_ERR.
 */
dom_exception _dom_text_event_get_data(dom_text_event *evt, 
		dom_string **data)
{
	*data = evt->data;
	dom_string_ref(*data);

	return DOM_NO_ERR;
}

/**
 * Initialise the TextEvent
 *
 * \param evt         The Event object
 * \param type        The type of this UIEvent
 * \param bubble      Whether this event can bubble
 * \param cancelable  Whether this event is cancelable
 * \param view        The AbstractView of this UIEvent
 * \param data        The text data
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_text_event_init(dom_text_event *evt,
		dom_string *type, bool bubble, bool cancelable, 
		struct dom_abstract_view *view, dom_string *data)
{
	evt->data = data;
	dom_string_ref(data);

	return _dom_ui_event_init(&evt->base, type, bubble, cancelable,
			view, 0);
}

/**
 * Initialise the TextEvent with namespace
 *
 * \param evt         The Event object
 * \param namespace   The namespace of this Event
 * \param type        The type of this UIEvent
 * \param bubble      Whether this event can bubble
 * \param cancelable  Whether this event is cancelable
 * \param view        The AbstractView of this UIEvent
 * \param data        The text data
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_text_event_init_ns(dom_text_event *evt, 
		dom_string *namespace_name, dom_string *type,
		bool bubble, bool cancelable, struct dom_abstract_view *view, 
		dom_string *data)
{
	evt->data = data;
	dom_string_ref(data);

	return _dom_ui_event_init_ns(&evt->base, namespace_name, type, bubble,
			cancelable, view, 0);
}

