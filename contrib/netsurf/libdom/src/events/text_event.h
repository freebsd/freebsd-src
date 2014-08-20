/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_events_text_event_h_
#define dom_internal_events_text_event_h_

#include <dom/events/text_event.h>

#include "events/ui_event.h"

/**
 * The TextEvent
 */
struct dom_text_event {
	struct dom_ui_event base;
	dom_string *data;
};

/* Constructor */
dom_exception _dom_text_event_create(struct dom_document *doc, 
		struct dom_text_event **evt);

/* Destructor */
void _dom_text_event_destroy(struct dom_text_event *evt);

/* Initialise function */
dom_exception _dom_text_event_initialise(struct dom_document *doc, 
		struct dom_text_event *evt);

/* Finalise function */
void _dom_text_event_finalise(struct dom_text_event *evt);

#endif

