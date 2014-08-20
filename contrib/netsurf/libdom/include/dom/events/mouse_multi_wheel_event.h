/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_events_mouse_multi_wheel_event_h_
#define dom_events_mouse_multi_wheel_event_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>
#include <dom/events/event_target.h>

struct dom_abstract_view;

typedef struct dom_mouse_multi_wheel_event dom_mouse_multi_wheel_event;

dom_exception _dom_mouse_multi_wheel_event_get_wheel_delta_x(
		dom_mouse_multi_wheel_event *evt, int32_t *x);
#define dom_mouse_multi_wheel_event_get_wheel_delta_x(e, x) \
		_dom_mouse_multi_wheel_event_get_wheel_delta_x( \
		(dom_mouse_multi_wheel_event *) (e), (int32_t *) (x))

dom_exception _dom_mouse_multi_wheel_event_get_wheel_delta_y(
		dom_mouse_multi_wheel_event *evt, int32_t *y);
#define dom_mouse_multi_wheel_event_get_wheel_delta_y(e, y) \
		_dom_mouse_multi_wheel_event_get_wheel_delta_y( \
		(dom_mouse_multi_wheel_event *) (e), (int32_t *) (y))

dom_exception _dom_mouse_multi_wheel_event_get_wheel_delta_z(
		dom_mouse_multi_wheel_event *evt, int32_t *z);
#define dom_mouse_multi_wheel_event_get_wheel_delta_z(e, z) \
		_dom_mouse_multi_wheel_event_get_wheel_delta_z( \
		(dom_mouse_multi_wheel_event *) (e), (int32_t *) (z))

dom_exception _dom_mouse_multi_wheel_event_init_ns(
		dom_mouse_multi_wheel_event *evt, dom_string *namespace,
		dom_string *type,  bool bubble, bool cancelable,
		struct dom_abstract_view *view, int32_t detail, int32_t screen_x,
		int32_t screen_y, int32_t client_x, int32_t client_y,
		unsigned short button, dom_event_target *et,
		dom_string *modifier_list, int32_t wheel_delta_x,
		int32_t wheel_delta_y, int32_t wheel_delta_z);
#define dom_mouse_multi_wheel_event_init_ns(e, n, t, b, c, v, \
		d, sx, sy, cx, cy, button, et, ml, x, y, z) \
		_dom_mouse_multi_wheel_event_init_ns( \
		(dom_mouse_multi_wheel_event *) (e), (dom_string *) (n),\
		(dom_string *) (t), (bool) (b), (bool) (c), \
		(struct dom_abstract_view *) (v), (int32_t) (d), (int32_t) (sx), \
		(int32_t) (sy), (int32_t) (cx), (int32_t) (cy),\
		(unsigned short) (button), (dom_event_target *) (et),\
		(dom_string *) (ml), (int32_t) (x), (int32_t) (y), (int32_t) (z))

#endif
