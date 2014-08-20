/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_events_dispatch_h_
#define dom_internal_events_dispatch_h_

#include <dom/core/document.h>
#include <dom/events/event.h>
#include <dom/events/mutation_event.h>

/* Dispatch a DOMNodeInserted/DOMNodeRemoved event */
dom_exception __dom_dispatch_node_change_event(dom_document *doc,
		dom_event_target *et, dom_event_target *related, 
		dom_mutation_type change, bool *success);
#define _dom_dispatch_node_change_event(doc, et, related, change, success) \
	__dom_dispatch_node_change_event((dom_document *) (doc), \
			(dom_event_target *) (et), \
			(dom_event_target *) (related), \
			(dom_mutation_type) (change), \
			(bool *) (success))

/* Dispatch a DOMNodeInsertedIntoDocument/DOMNodeRemovedFromDocument event */
dom_exception __dom_dispatch_node_change_document_event(dom_document *doc,
		dom_event_target *et, dom_mutation_type change, bool *success);
#define _dom_dispatch_node_change_document_event(doc, et, change, success) \
	__dom_dispatch_node_change_document_event((dom_document *) (doc), \
			(dom_event_target *) (et), \
			(dom_mutation_type) (change), \
			(bool *) (success))

/* Dispatch a DOMCharacterDataModified event */
dom_exception __dom_dispatch_characterdata_modified_event(
		dom_document *doc, dom_event_target *et,
		dom_string *prev, dom_string *new, bool *success);
#define _dom_dispatch_characterdata_modified_event(doc, et, \
		prev, new, success) \
	__dom_dispatch_characterdata_modified_event((dom_document *) (doc), \
			(dom_event_target *) (et), \
			(dom_string *) (prev), \
			(dom_string *) (new), \
			(bool *) (success))

/* Dispatch a DOMAttrModified event */
dom_exception __dom_dispatch_attr_modified_event(dom_document *doc,
		dom_event_target *et, dom_string *prev,
		dom_string *new, dom_event_target *related,
		dom_string *attr_name, dom_mutation_type change,
		bool *success);
#define _dom_dispatch_attr_modified_event(doc, et, prev, new, \
		related, attr_name, change, success) \
	__dom_dispatch_attr_modified_event((dom_document *) (doc), \
			(dom_event_target *) (et), \
			(dom_string *) (prev), \
			(dom_string *) (new), \
			(dom_event_target *) (related), \
			(dom_string *) (attr_name), \
			(dom_mutation_type) (change), \
			(bool *) (success))

/* Dispatch a DOMSubtreeModified event */
dom_exception __dom_dispatch_subtree_modified_event(dom_document *doc,
		dom_event_target *et, bool *success);
#define _dom_dispatch_subtree_modified_event(doc, et, success) \
	__dom_dispatch_subtree_modified_event((dom_document *) (doc), \
			(dom_event_target *) (et), \
			(bool *) (success))

/* Dispatch a generic event */
dom_exception _dom_dispatch_generic_event(dom_document *doc,
		dom_event_target *et, dom_string *event_name,
		bool bubble, bool cancelable, bool *success);

#endif
