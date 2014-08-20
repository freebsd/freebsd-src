/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_events_mutation_name_event_h_
#define dom_events_mutation_name_event_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>

struct dom_node;

typedef struct dom_mutation_name_event dom_mutation_name_event;

dom_exception _dom_mutation_name_event_get_prev_namespace(
		dom_mutation_name_event *evt, dom_string **namespace);
#define dom_mutation_name_event_get_prev_namespace(e, n) \
		_dom_mutation_name_event_get_prev_namespace(\
		(dom_mutation_name_event *) (e), (dom_string **) (n))

dom_exception _dom_mutation_name_event_get_prev_node_name(
		dom_mutation_name_event *evt, dom_string **name);
#define dom_mutation_name_event_get_prev_node_name(e, n) \
		_dom_mutation_name_event_get_prev_node_name(\
		(dom_mutation_name_event *) (e), (dom_string **) (n))

dom_exception _dom_mutation_name_event_init(dom_mutation_name_event *evt, 
		dom_string *type, bool bubble, bool cancelable,
		struct dom_node *node, dom_string *prev_ns,
		dom_string *prev_name); 
#define dom_mutation_name_event_init(e, t, b, c, n, p, pn) \
		_dom_mutation_name_event_init((dom_mutation_name_event *) (e), \
		(dom_string *) (t), (bool) (b), (bool) (c), \
		(struct dom_node *) (n), (dom_string *) (p), \
		(dom_string *) (pn))

dom_exception _dom_mutation_name_event_init_ns(dom_mutation_name_event *evt, 
		dom_string *namespace, dom_string *type,
		bool bubble, bool cancelable, struct dom_node *node,
		dom_string *prev_ns, dom_string *prev_name);
#define dom_mutation_name_event_init_ns(e, n, t, b, c, nv, p, pn) \
		_dom_mutation_name_event_init_ns(\
		(dom_mutation_name_event *) (e), (dom_string *) (n),\
		(dom_string *) (t), (bool) (b), (bool) (c),\
		(struct dom_node *) (nv), (dom_string *) (p), \
		(dom_string *) (pn))

#endif


