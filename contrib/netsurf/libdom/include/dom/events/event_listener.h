/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_events_event_listener_h_
#define dom_events_event_listener_h_

#include <dom/core/exceptions.h>

struct dom_document;
struct dom_event;

typedef void (*handle_event)(struct dom_event *evt, void *pw);

typedef struct dom_event_listener dom_event_listener;

dom_exception dom_event_listener_create(struct dom_document *doc,
		handle_event handler, void *pw, dom_event_listener **listener);

void dom_event_listener_ref(dom_event_listener *listener);
void dom_event_listener_unref(dom_event_listener *listener);

#endif

