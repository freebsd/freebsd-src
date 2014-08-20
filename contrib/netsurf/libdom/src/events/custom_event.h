/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_interntal_events_custom_event_h_
#define dom_interntal_events_custom_event_h_

#include <dom/events/custom_event.h>

#include "events/event.h"

struct dom_custom_event {
	struct dom_event base;
	void *detail;
};

/* Constructor */
dom_exception _dom_custom_event_create(struct dom_document *doc, 
		struct dom_custom_event **evt);

/* Destructor */
void _dom_custom_event_destroy(struct dom_custom_event *evt);

/* Initialise function */
dom_exception _dom_custom_event_initialise(struct dom_document *doc, 
		struct dom_custom_event *evt);

/* Finalise function */
void _dom_custom_event_finalise(struct dom_custom_event *evt);

#endif
