/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_events_mouse_event_h_
#define dom_events_mouse_event_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>
#include <dom/events/event_target.h>

struct dom_abstract_view;

typedef struct dom_mouse_event dom_mouse_event;

dom_exception _dom_mouse_event_get_screen_x(dom_mouse_event *evt,
		int32_t *x);
#define dom_mouse_event_get_screen_x(e, x) _dom_mouse_event_get_screen_x(\
		(dom_mouse_event *) (e), (int32_t *) (x))

dom_exception _dom_mouse_event_get_screen_y(dom_mouse_event *evt,
		int32_t *y);
#define dom_mouse_event_get_screen_y(e, y) _dom_mouse_event_get_screen_y(\
		(dom_mouse_event *) (e), (int32_t *) (y))

dom_exception _dom_mouse_event_get_client_x(dom_mouse_event *evt,
		int32_t *x);
#define dom_mouse_event_get_client_x(e, x) _dom_mouse_event_get_client_x(\
		(dom_mouse_event *) (e), (int32_t *) (x))

dom_exception _dom_mouse_event_get_client_y(dom_mouse_event *evt,
		int32_t *y);
#define dom_mouse_event_get_client_y(e, y) _dom_mouse_event_get_client_y(\
		(dom_mouse_event *) (e), (int32_t *) (y))

dom_exception _dom_mouse_event_get_ctrl_key(dom_mouse_event *evt,
		bool *key);
#define dom_mouse_event_get_ctrl_key(e, k) _dom_mouse_event_get_ctrl_key( \
		(dom_mouse_event *) (e), (bool *) (k))

dom_exception _dom_mouse_event_get_shift_key(dom_mouse_event *evt,
		bool *key);
#define dom_mouse_event_get_shift_key(e, k) _dom_mouse_event_get_shift_key( \
		(dom_mouse_event *) (e), (bool *) (k))

dom_exception _dom_mouse_event_get_alt_key(dom_mouse_event *evt,
		bool *key);
#define dom_mouse_event_get_alt_key(e, k) _dom_mouse_event_get_alt_key( \
		(dom_mouse_event *) (e), (bool *) (k))

dom_exception _dom_mouse_event_get_meta_key(dom_mouse_event *evt,
		bool *key);
#define dom_mouse_event_get_meta_key(e, k) _dom_mouse_event_get_meta_key( \
		(dom_mouse_event *) (e), (bool *) (k))

dom_exception _dom_mouse_event_get_button(dom_mouse_event *evt,
		unsigned short *button);
#define dom_mouse_event_get_button(e, b) _dom_mouse_event_get_button(\
		(dom_mouse_event *) (e), (unsigned short *) (b))

dom_exception _dom_mouse_event_get_related_target(dom_mouse_event *evt,
		dom_event_target **et);
#define dom_mouse_event_get_related_target(e, t) \
		_dom_mouse_event_get_related_target((dom_mouse_event *) (e),\
		(dom_event_target **) (t))

dom_exception _dom_mouse_event_get_modifier_state(dom_mouse_event *evt,
		dom_string *m, bool *state);
#define dom_mouse_event_get_modifier_state(e, m, s) \
		_dom_mouse_event_get_modifier_state((dom_mouse_event *) (e), \
		(dom_string *) (m), (bool *) (s))

dom_exception _dom_mouse_event_init(dom_mouse_event *evt, 
		dom_string *type, bool bubble, bool cancelable, 
		struct dom_abstract_view *view, int32_t detail, int32_t screen_x,
		int32_t screen_y, int32_t client_x, int32_t client_y, bool ctrl,
		bool alt, bool shift, bool meta, unsigned short button,
		dom_event_target *et);
#define dom_mouse_event_init(e, t, b, c, v, d, sx, sy, cx, cy, ctrl, alt, \
		shift, meta, button, et) \
		_dom_mouse_event_init((dom_mouse_event *) (e), \
		(dom_string *) (t), (bool) (b), (bool) (c),\
		(struct dom_abstract_view *) (v), (int32_t) (d), (int32_t) (sx), \
		(int32_t) (sy), (int32_t) (cx), (int32_t) (cy), (bool) (ctrl),\
		(bool) (alt), (bool) (shift), (bool) (meta), \
		(unsigned short) (button), (dom_event_target *) (et))

dom_exception _dom_mouse_event_init_ns(dom_mouse_event *evt,
		dom_string *namespace, dom_string *type,
		bool bubble, bool cancelable, struct dom_abstract_view *view,
		int32_t detail, int32_t screen_x, int32_t screen_y, int32_t client_x,
		int32_t client_y, bool ctrl, bool alt, bool shift, bool meta,
		unsigned short button, dom_event_target *et);
#define dom_mouse_event_init_ns(e, n, t, b, c, v, d, sx, sy, cx, cy, ctrl, alt,\
		shift, meta, button, et) \
		_dom_mouse_event_init_ns((dom_mouse_event *) (e), \
		(dom_string *) (n), (dom_string *) (t),\
		(bool) (b), (bool) (c), (struct dom_abstract_view *) (v),\
		(int32_t) (d), (int32_t) (sx), (int32_t) (sy), (int32_t) (cx),\
		(int32_t) (cy), (bool) (ctrl), (bool) (alt), (bool) (shift),\
		(bool) (meta), (unsigned short) (button),\
		(dom_event_target *) (et))

#endif
