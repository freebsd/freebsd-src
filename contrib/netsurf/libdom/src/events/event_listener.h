/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_events_event_listener_h_
#define dom_internal_events_event_listener_h_

#include <dom/events/event_listener.h>

#include "utils/list.h"

/**
 * The EventListener class
 */
struct dom_event_listener {
	handle_event handler;	/**< The event handler function */
	void *pw;		/**< The private data of this listener */

	unsigned int refcnt;	/**< The reference count of this listener */
	struct dom_document *doc;
			/**< The document which create this listener */
};

#endif

