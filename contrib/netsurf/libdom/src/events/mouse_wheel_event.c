/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdlib.h>

#include "events/mouse_wheel_event.h"
#include "events/keyboard_event.h"
#include "core/document.h"

#include "utils/utils.h"

static void _virtual_dom_mouse_wheel_event_destroy(struct dom_event *evt);

static struct dom_event_private_vtable _event_vtable = {
	_virtual_dom_mouse_wheel_event_destroy
};

/* Constructor */
dom_exception _dom_mouse_wheel_event_create(struct dom_document *doc, 
		struct dom_mouse_wheel_event **evt)
{
	*evt = malloc(sizeof(dom_mouse_wheel_event));
	if (*evt == NULL) 
		return DOM_NO_MEM_ERR;
	
	((struct dom_event *) *evt)->vtable = &_event_vtable;

	return _dom_mouse_wheel_event_initialise(doc, 
			(dom_mouse_wheel_event *) *evt);
}

/* Destructor */
void _dom_mouse_wheel_event_destroy(struct dom_mouse_wheel_event *evt)
{
	_dom_mouse_wheel_event_finalise((dom_ui_event *) evt);

	free(evt);
}

/* Initialise function */
dom_exception _dom_mouse_wheel_event_initialise(struct dom_document *doc,
		struct dom_mouse_wheel_event *evt)
{
	return _dom_mouse_event_initialise(doc, (dom_mouse_event *) evt);
}

/* The virtual destroy function */
void _virtual_dom_mouse_wheel_event_destroy(struct dom_event *evt)
{
	_dom_mouse_wheel_event_destroy((dom_mouse_wheel_event *) evt);
}

/*----------------------------------------------------------------------*/
/* The public API */

/**
 * Get wheelDelta
 *
 * \param evt  The Event object
 * \param d    The wheelDelta
 * \return DOM_NO_ERR.
 */
dom_exception _dom_mouse_wheel_event_get_wheel_delta(
		dom_mouse_wheel_event *evt, int32_t *d)
{
	*d = evt->delta;

	return DOM_NO_ERR;
}

/**
 * Intialise this event with namespace
 *
 * \param evt            The Event object
 * \param namespace      The namespace of this event
 * \param type           The event's type
 * \param bubble         Whether this is a bubbling event
 * \param cancelable     Whether this is a cancelable event
 * \param view           The AbstractView associated with this event
 * \param detail         The detail information of this mouse event
 * \param screen_x       The x position of the mouse pointer in screen
 * \param screen_y       The y position of the mouse pointer in screen
 * \param client_x       The x position of the mouse pointer in client window
 * \param client_y       The y position of the mouse pointer in client window
 * \param button         The mouse button pressed
 * \param et             The related target of this event, may be NULL
 * \param modifier_list  The string contains the modifier identifier strings
 * \param wheel_delta    The wheelDelta
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_mouse_wheel_event_init_ns(
		dom_mouse_wheel_event *evt, dom_string *namespace,
		dom_string *type,  bool bubble, bool cancelable,
		struct dom_abstract_view *view, int32_t detail, int32_t screen_x,
		int32_t screen_y, int32_t client_x, int32_t client_y,
		unsigned short button, dom_event_target *et,
		dom_string *modifier_list, int32_t wheel_delta)
{
	dom_exception err;
	dom_mouse_event *e = (dom_mouse_event *) evt;
	evt->delta = wheel_delta;

	err = _dom_parse_modifier_list(modifier_list, &e->modifier_state);
	if (err != DOM_NO_ERR)
		return err;

	return _dom_mouse_event_init_ns(&evt->base, namespace, type, bubble,
			cancelable, view, detail ,screen_x, screen_y,
			client_x, client_y, false, false, false, false,
			button, et);
}

