/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_interntal_events_mouse_event_h_
#define dom_interntal_events_mouse_event_h_

#include <dom/events/mouse_event.h>

#include "events/ui_event.h"

/**
 * The mouse event
 */
struct dom_mouse_event {
	struct dom_ui_event base;	/**< Base class */

	int32_t sx;	/**< ScreenX */
	int32_t sy;	/**< ScreenY */
	int32_t cx;	/**< ClientX */
	int32_t cy;	/**< ClientY */

	uint32_t modifier_state;	/**< The modifier keys state */

	unsigned short button;	/**< Which button is clicked */
	dom_event_target *related_target;	/**< The related target */
};

/* Constructor */
dom_exception _dom_mouse_event_create(struct dom_document *doc, 
		struct dom_mouse_event **evt);

/* Destructor */
void _dom_mouse_event_destroy(struct dom_mouse_event *evt);

/* Initialise function */
dom_exception _dom_mouse_event_initialise(struct dom_document *doc, 
		struct dom_mouse_event *evt);

/* Finalise function */
#define _dom_mouse_event_finalise _dom_ui_event_finalise


#endif

