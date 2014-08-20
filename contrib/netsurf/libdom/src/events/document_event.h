/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_events_document_event_h_
#define dom_internal_events_document_event_h_

#include <dom/events/document_event.h>

struct dom_event_listener;
struct lwc_string_s;
struct dom_document;

/**
 * Type of Events
 */
typedef enum {
	DOM_EVENT = 0,
	DOM_CUSTOM_EVENT,
	DOM_UI_EVENT,
	DOM_TEXT_EVENT,
	DOM_KEYBOARD_EVENT,
	DOM_MOUSE_EVENT,
	DOM_MOUSE_MULTI_WHEEL_EVENT,
	DOM_MOUSE_WHEEL_EVENT,
	DOM_MUTATION_EVENT,
	DOM_MUTATION_NAME_EVENT,

	DOM_EVENT_COUNT
} dom_event_type;

/**
 * The DocumentEvent internal class
 */
struct dom_document_event_internal {
	dom_events_default_action_fetcher actions;
			/**< The default action fetecher */
	void *actions_ctx; /**< The default action fetcher context */
	struct lwc_string_s *event_types[DOM_EVENT_COUNT];
			/**< Events type names */
};

typedef struct dom_document_event_internal dom_document_event_internal;

/**
 * Constructor and destructor: Since this object is not intended to be
 * allocated alone, it should be embedded into the Document object, there 
 * is no constructor and destructor for it.
 */

/* Initialise this DocumentEvent */
dom_exception _dom_document_event_internal_initialise(struct dom_document *doc,
		dom_document_event_internal *dei, 
		dom_events_default_action_fetcher actions,
		void *actions_ctx);

/* Finalise this DocumentEvent */
void _dom_document_event_internal_finalise(struct dom_document *doc,
		dom_document_event_internal *dei);

#endif
