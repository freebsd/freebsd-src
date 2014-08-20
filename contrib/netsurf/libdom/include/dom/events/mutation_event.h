/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_events_mutation_event_h_
#define dom_events_mutation_event_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>

struct dom_node;

typedef enum {
	DOM_MUTATION_MODIFICATION = 1,
	DOM_MUTATION_ADDITION = 2,
	DOM_MUTATION_REMOVAL = 3
} dom_mutation_type;

typedef struct dom_mutation_event dom_mutation_event;

dom_exception _dom_mutation_event_get_related_node(dom_mutation_event *evt,
		struct dom_node **node);
#define dom_mutation_event_get_related_node(e, n) \
		_dom_mutation_event_get_related_node(\
		(dom_mutation_event *) (e), (struct dom_node **) (n))

dom_exception _dom_mutation_event_get_prev_value(dom_mutation_event *evt,
		dom_string **ret);
#define dom_mutation_event_get_prev_value(e, r) \
		_dom_mutation_event_get_prev_value((dom_mutation_event *) (e), \
		(dom_string **) (r))

dom_exception _dom_mutation_event_get_new_value(dom_mutation_event *evt,
		dom_string **ret);
#define dom_mutation_event_get_new_value(e, r) \
		_dom_mutation_event_get_new_value((dom_mutation_event *) (e), \
		(dom_string **) (r))

dom_exception _dom_mutation_event_get_attr_name(dom_mutation_event *evt,
		dom_string **ret);
#define dom_mutation_event_get_attr_name(e, r) \
		_dom_mutation_event_get_attr_name((dom_mutation_event *) (e), \
		(dom_string **) (r))

dom_exception _dom_mutation_event_get_attr_change(dom_mutation_event *evt,
		dom_mutation_type *type);
#define dom_mutation_event_get_attr_change(e, t) \
		_dom_mutation_event_get_attr_change((dom_mutation_event *) (e),\
		(dom_mutation_type *) (t))

dom_exception _dom_mutation_event_init(dom_mutation_event *evt, 
		dom_string *type, bool bubble, bool cancelable,
		struct dom_node *node, dom_string *prev_value,
		dom_string *new_value, dom_string *attr_name,
		dom_mutation_type change);
#define dom_mutation_event_init(e, t, b, c, n, p, nv, a, ch) \
		_dom_mutation_event_init((dom_mutation_event *) (e), \
		(dom_string *) (t), (bool) (b), (bool) (c), \
		(struct dom_node *) (n), (dom_string *) (p), \
		(dom_string *) (nv), (dom_string *) (a), \
		(dom_mutation_type) (ch))

dom_exception _dom_mutation_event_init_ns(dom_mutation_event *evt,
		dom_string *namespace, dom_string *type,
		bool bubble, bool cancelable, struct dom_node *node,
		dom_string *prev_value, dom_string *new_value,
		dom_string *attr_name, dom_mutation_type change);
#define dom_mutation_event_init_ns(e, n, t, b, c, nd, p, nv, a, ch) \
		_dom_mutation_event_init_ns((dom_mutation_event *) (e), \
		(dom_string *) (n), (dom_string *) (t),\
		(bool) (b), (bool) (c), (struct dom_node *) (nd), \
		(dom_string *) (p), (dom_string *) (nv),\
		(dom_string *) (a), (dom_mutation_type) (ch))

#endif

