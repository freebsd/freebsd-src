/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_core_implementation_h_
#define dom_core_implementation_h_

#include <stdbool.h>

#include <dom/core/exceptions.h>
#include <dom/events/document_event.h>
#include <dom/functypes.h>

struct dom_document;
struct dom_document_type;

typedef const char dom_implementation;

typedef enum dom_implementation_type {
	DOM_IMPLEMENTATION_CORE = 0,
	DOM_IMPLEMENTATION_XML  = (1 << 0),	/* not implemented */
	DOM_IMPLEMENTATION_HTML = (1 << 1),

	DOM_IMPLEMENTATION_ALL  = DOM_IMPLEMENTATION_CORE |
				  DOM_IMPLEMENTATION_XML  |
				  DOM_IMPLEMENTATION_HTML
} dom_implementation_type;

dom_exception dom_implementation_has_feature(
		const char *feature, const char *version,
		bool *result);

dom_exception dom_implementation_create_document_type(
		const char *qname,
		const char *public_id, const char *system_id,
		struct dom_document_type **doctype);

dom_exception dom_implementation_create_document(
		uint32_t impl_type,
		const char *namespace, const char *qname,
		struct dom_document_type *doctype,
		dom_events_default_action_fetcher daf,
		void *daf_ctx,
		struct dom_document **doc);

dom_exception dom_implementation_get_feature(
		const char *feature, const char *version,
		void **object);

#endif
