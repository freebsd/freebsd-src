/* * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_events_mouse_wheel_event_h_
#define dom_events_mouse_wheel_event_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>
#include <dom/events/event_target.h>

struct dom_abstract_view;

typedef struct dom_mouse_wheel_event dom_mouse_wheel_event;

dom_exception _dom_mouse_wheel_event_get_wheel_delta(
		dom_mouse_wheel_event *evt, int32_t *d);
#define dom_mouse_wheel_event_get_wheel_delta(e, d) \
		_dom_mouse_wheel_event_get_wheel_delta( \
		(dom_mouse_wheel_event *) (e), (int32_t *) (d))

dom_exception _dom_mouse_wheel_event_init_ns(
		dom_mouse_wheel_event *evt, dom_string *namespace,
		dom_string *type,  bool bubble, bool cancelable,
		struct dom_abstract_view *view, int32_t detail, int32_t screen_x,
		int32_t screen_y, int32_t client_x, int32_t client_y,
		unsigned short button, dom_event_target *et,
		dom_string *modifier_list, int32_t wheel_delta);
#define dom_mouse_wheel_event_init_ns(e, n, t, b, c, v, \
		d, sx, sy, cx, cy, button, et, ml, dt) \
		_dom_mouse_wheel_event_init_ns((dom_mouse_wheel_event *) (e), \
		(dom_string *) (n), (dom_string *) (t), \
		(bool) (b), (bool) (c), (struct dom_abstract_view *) (v),\
		(int32_t) (d), (int32_t) (sx), (int32_t) (sy), (int32_t) (cx), (int32_t) (cy),\
		(unsigned short) (button), (dom_event_target *) (et),\
		(dom_string *) (ml), (int32_t) (dt))

#endif

