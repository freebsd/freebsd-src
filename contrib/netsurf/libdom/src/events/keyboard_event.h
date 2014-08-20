/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_interntal_events_keyboard_event_h_
#define dom_interntal_events_keyboard_event_h_

#include <dom/events/keyboard_event.h>

#include "events/ui_event.h"

/**
 * The keyboard event
 */
struct dom_keyboard_event {
	struct dom_ui_event base;	/**< The base class */

	dom_string *key_ident;	/**< The identifier of the key in this 
					 * event, please refer:
					 * http://www.w3.org/TR/DOM-Level-3-Events/keyset.html#KeySet-Set
					 * for detail
					 */

	dom_key_location key_loc;	/**< Indicate the location of the key on
					 * the keyboard
					 */

	uint32_t modifier_state;	/**< The modifier keys state */
};

/* Constructor */
dom_exception _dom_keyboard_event_create(struct dom_document *doc, 
		struct dom_keyboard_event **evt);

/* Destructor */
void _dom_keyboard_event_destroy(struct dom_keyboard_event *evt);

/* Initialise function */
dom_exception _dom_keyboard_event_initialise(struct dom_document *doc, 
		struct dom_keyboard_event *evt);

/* Finalise function */
void _dom_keyboard_event_finalise(struct dom_keyboard_event *evt);


/* Parse the modifier list string to corresponding bool variable state */
dom_exception _dom_parse_modifier_list(dom_string *modifier_list,
		uint32_t *modifier_state);

#endif
