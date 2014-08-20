/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_events_event_h_
#define dom_events_event_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>
#include <dom/events/event_target.h>

typedef enum {
	DOM_CAPTURING_PHASE = 1,
	DOM_AT_TARGET       = 2,
	DOM_BUBBLING_PHASE  = 3 
} dom_event_flow_phase;

typedef struct dom_event dom_event;

/* The ref/unref methods define */
void _dom_event_ref(dom_event *evt);
#define dom_event_ref(n) _dom_event_ref((dom_event *) (n))
void _dom_event_unref(dom_event *evt);
#define dom_event_unref(n) _dom_event_unref((dom_event *) (n))

dom_exception _dom_event_get_type(dom_event *evt, dom_string **type);
#define dom_event_get_type(e, t) _dom_event_get_type((dom_event *) (e), \
		(dom_string **) (t))

dom_exception _dom_event_get_target(dom_event *evt, dom_event_target **target);
#define dom_event_get_target(e, t) _dom_event_get_target((dom_event *) (e), \
		(dom_event_target **) (t))

dom_exception _dom_event_get_current_target(dom_event *evt,
		dom_event_target **current);
#define dom_event_get_current_target(e, c) _dom_event_get_current_target(\
		(dom_event *) (e), (dom_event_target **) (c))

dom_exception _dom_event_get_bubbles(dom_event *evt, bool *bubbles);
#define dom_event_get_bubbles(e, b) _dom_event_get_bubbles((dom_event *) (e), \
		(bool *) (b))

dom_exception _dom_event_get_cancelable(dom_event *evt, bool *cancelable);
#define dom_event_get_cancelable(e, c) _dom_event_get_cancelable(\
		(dom_event *) (e), (bool *) (c))

dom_exception _dom_event_get_timestamp(dom_event *evt,
		unsigned int *timestamp);
#define dom_event_get_timestamp(e, t) _dom_event_get_timestamp(\
		(dom_event *) (e), (unsigned int *) (t))

dom_exception _dom_event_stop_propagation(dom_event *evt);
#define dom_event_stop_propagation(e) _dom_event_stop_propagation(\
		(dom_event *) (e))

dom_exception _dom_event_prevent_default(dom_event *evt);
#define dom_event_prevent_default(e) _dom_event_prevent_default(\
		(dom_event *) (e))

dom_exception _dom_event_init(dom_event *evt, dom_string *type, 
		bool bubble, bool cancelable);
#define dom_event_init(e, t, b, c) _dom_event_init((dom_event *) (e), \
		(dom_string *) (t), (bool) (b), (bool) (c))

dom_exception _dom_event_get_namespace(dom_event *evt,
		dom_string **namespace);
#define dom_event_get_namespace(e, n) _dom_event_get_namespace(\
		(dom_event *) (e), (dom_string **) (n))

dom_exception _dom_event_is_custom(dom_event *evt, bool *custom);
#define dom_event_is_custom(e, c) _dom_event_is_custom((dom_event *) (e), \
		(bool *) (c))
	
dom_exception _dom_event_stop_immediate_propagation(dom_event *evt);
#define dom_event_stop_immediate_propagation(e)  \
		_dom_event_stop_immediate_propagation((dom_event *) (e))

dom_exception _dom_event_is_default_prevented(dom_event *evt, bool *prevented);
#define dom_event_is_default_prevented(e, p) \
		_dom_event_is_default_prevented((dom_event *) (e), (bool *) (p))

dom_exception _dom_event_init_ns(dom_event *evt, dom_string *namespace,
		dom_string *type, bool bubble, bool cancelable);
#define dom_event_init_ns(e, n, t, b, c) _dom_event_init_ns( \
		(dom_event *) (e), (dom_string *) (n), \
		(dom_string *) (t), (bool) (b), (bool) (c))

#endif
