/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_events_ui_event_h_
#define dom_events_ui_event_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>

struct dom_abstract_view;

typedef struct dom_ui_event dom_ui_event;

dom_exception _dom_ui_event_get_view(dom_ui_event *evt, 
		struct dom_abstract_view **view);
#define dom_ui_event_get_view(e, v) _dom_ui_event_get_view( \
		(dom_ui_event *) (e), (struct dom_abstract_view **) (v))

dom_exception _dom_ui_event_get_detail(dom_ui_event *evt,
		int32_t *detail);
#define dom_ui_event_get_detail(e, d) _dom_ui_event_get_detail(\
		(dom_ui_event *) (e), (int32_t *) (d))

dom_exception _dom_ui_event_init(dom_ui_event *evt, dom_string *type, 
		bool bubble, bool cancelable, struct dom_abstract_view *view,
		int32_t detail);
#define dom_ui_event_init(e, t, b, c, v, d) _dom_ui_event_init( \
		(dom_ui_event *) (e), (dom_string *) (t), (bool) (b), \
		(bool) (c), (struct dom_abstract_view *) (v), (int32_t) (d))

dom_exception _dom_ui_event_init_ns(dom_ui_event *evt,
		dom_string *namespace, dom_string *type,
		bool bubble, bool cancelable, struct dom_abstract_view *view,
		int32_t detail);
#define dom_ui_event_init_ns(e, n, t, b, c, v, d) _dom_ui_event_init_ns( \
		(dom_ui_event *) (e), (dom_string *) (n), \
		(dom_string *) (t), (bool) (b), (bool) (c), \
		(struct dom_abstract_view *) (v), (int32_t) (d))

#endif
