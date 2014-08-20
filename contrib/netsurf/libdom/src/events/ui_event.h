/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_interntal_events_ui_event_h_
#define dom_interntal_events_ui_event_h_

#include <dom/events/ui_event.h>

#include "events/event.h"

/**
 * The modifier key state
 */
typedef enum {
	DOM_MOD_CTRL = (1<<0),
	DOM_MOD_META = (1<<1),
	DOM_MOD_SHIFT = (1<<2),
	DOM_MOD_ALT = (1<<3),
	DOM_MOD_ALT_GRAPH = (1<<4),
	DOM_MOD_CAPS_LOCK = (1<<5),
	DOM_MOD_NUM_LOCK = (1<<6),
	DOM_MOD_SCROLL = (1<<7)
} dom_modifier_key;

/**
 * The UIEvent
 */
struct dom_ui_event {
	struct dom_event base;	/**< The base class */
	struct dom_abstract_view *view;	/**< The AbstractView */
	int32_t detail;	/**< Some private data for this event */
};

/* Constructor */
dom_exception _dom_ui_event_create(struct dom_document *doc, 
		struct dom_ui_event **evt);

/* Destructor */
void _dom_ui_event_destroy(struct dom_ui_event *evt);

/* Initialise function */
dom_exception _dom_ui_event_initialise(struct dom_document *doc, 
		struct dom_ui_event *evt);

/* Finalise function */
void _dom_ui_event_finalise(struct dom_ui_event *evt);

#endif
