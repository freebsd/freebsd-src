/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_events_event_target_h_
#define dom_internal_events_event_target_h_

#include <dom/core/document.h>
#include <dom/events/event.h>
#include <dom/events/mutation_event.h>
#include <dom/events/event_target.h>
#include <dom/events/event_listener.h>

#include "events/dispatch.h"

#include "utils/list.h"

/**
 * Listener Entry
 */
struct listener_entry {
	struct list_entry list;	
		/**< The listener list registered at the same
		 * EventTarget */
	dom_string *type; /**< Event type */
	dom_event_listener *listener;	/**< The EventListener */
	bool capture;	/**< Whether this listener is in capture phase */
};

/**
 * EventTarget internal class
 */
struct dom_event_target_internal {
	struct listener_entry *listeners;	
			/**< The listeners of this EventTarget. */
};

typedef struct dom_event_target_internal dom_event_target_internal;

/* Entry for a EventTarget, used to record the bubbling list */
typedef struct dom_event_target_entry {
	struct list_entry entry;	/**< The list entry */
	dom_event_target *et;	/**< The node */
} dom_event_target_entry;

/**
 * Constructor and destructor: Since this object is not intended to be 
 * allocated alone, it should be embedded into the Node object, there is
 * no constructor and destructor for it.
 */

/* Initialise this EventTarget */
dom_exception _dom_event_target_internal_initialise(
		dom_event_target_internal *eti);

/* Finalise this EventTarget */
void _dom_event_target_internal_finalise(dom_event_target_internal *eti);

dom_exception _dom_event_target_add_event_listener(
		dom_event_target_internal *eti,
		dom_string *type, struct dom_event_listener *listener, 
		bool capture);

dom_exception _dom_event_target_remove_event_listener(
		dom_event_target_internal *eti,
		dom_string *type, struct dom_event_listener *listener, 
		bool capture);

dom_exception _dom_event_target_add_event_listener_ns(
		dom_event_target_internal *eti,
		dom_string *namespace, dom_string *type, 
		struct dom_event_listener *listener, bool capture);

dom_exception _dom_event_target_remove_event_listener_ns(
		dom_event_target_internal *eti,
		dom_string *namespace, dom_string *type, 
		struct dom_event_listener *listener, bool capture);

dom_exception _dom_event_target_dispatch(dom_event_target *et,
		dom_event_target_internal *eti, 
		struct dom_event *evt, dom_event_flow_phase phase,
		bool *success);

#endif
