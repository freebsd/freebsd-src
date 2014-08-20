/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdlib.h>

#include "events/event_listener.h"
#include "core/document.h"

/**
 * Create an EventListener
 *
 * \param doc       The document object
 * \param handler   The event function
 * \param pw        The private data
 * \param listener  The returned EventListener
 * \return DOM_NO_ERR on success, DOM_NO_MEM_ERR on memory exhaustion.
 */
dom_exception dom_event_listener_create(struct dom_document *doc,
		handle_event handler, void *pw, dom_event_listener **listener)
{
	dom_event_listener *ret = malloc(sizeof(dom_event_listener));
	if (ret == NULL)
		return DOM_NO_MEM_ERR;
	
	ret->handler = handler;
	ret->pw = pw;
	ret->refcnt = 1;
	ret->doc = doc;

	*listener = ret;

	return DOM_NO_ERR;
}

/**
 * Claim a new reference on the listener object
 *
 * \param listener  The EventListener object
 */
void dom_event_listener_ref(dom_event_listener *listener)
{
	listener->refcnt++;
}

/**
 * Release a reference on the listener object
 *
 * \param listener  The EventListener object
 */
void dom_event_listener_unref(dom_event_listener *listener)
{
	if (listener->refcnt > 0)
		listener->refcnt--;

	if (listener->refcnt == 0)
		free(listener);
}

