/*
 * Copyright 2010 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer windowing toolkit event processing.
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_plot_util.h>
#include <libnsfb_event.h>
#include <libnsfb_cursor.h>

#include "utils/utils.h"
#include "utils/log.h"
#include "css/css.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/image_data.h"

#include "widget.h"

/* exported function documented in fbtk.h */
void
fbtk_input(fbtk_widget_t *root, nsfb_event_t *event)
{
	fbtk_widget_t *input;

	root = fbtk_get_root_widget(root);

	/* obtain widget with input focus */
	input = root->u.root.input;
	if (input == NULL) {
		LOG(("No widget has input focus."));
		return; /* no widget with input */
	}

	fbtk_post_callback(input, FBTK_CBT_INPUT, event);
}

/* exported function documented in fbtk.h */
void
fbtk_click(fbtk_widget_t *widget, nsfb_event_t *event)
{
	fbtk_widget_t *root;
	fbtk_widget_t *clicked;
	nsfb_bbox_t cloc;
	int x, y;

	/* ensure we have the root widget */
	root = fbtk_get_root_widget(widget);

	nsfb_cursor_loc_get(root->u.root.fb, &cloc);

	clicked = fbtk_get_widget_at(root, cloc.x0, cloc.y0);

	if (clicked == NULL)
		return;

	if (fbtk_get_handler(clicked, FBTK_CBT_INPUT) != NULL) {
		fbtk_set_focus(clicked);
	}

	x = fbtk_get_absx(clicked);
	y = fbtk_get_absy(clicked);

	LOG(("clicked %p at %d,%d", clicked, x, y));

	/* post the click */
	fbtk_post_callback(clicked, FBTK_CBT_CLICK, event, cloc.x0 - x, cloc.y0 - y);
}

/* exported function documented in fbtk.h */
bool
fbtk_tgrab_pointer(fbtk_widget_t *widget)
{
	fbtk_widget_t *root;

	/* ensure we have the root widget */
	root = fbtk_get_root_widget(widget);

	if (root->u.root.grabbed == widget) {
		/* release pointer grab */
		root->u.root.grabbed = NULL;
		return true;
	} else if (root->u.root.grabbed == NULL) {
		/* set pointer grab */
		root->u.root.grabbed = widget;
		return true;
	}
	/* pointer was already grabbed */
	return false;
}

/* exported function documented in fbtk.h */
void
fbtk_warp_pointer(fbtk_widget_t *widget, int x, int y, bool relative)
{
	fbtk_widget_t *root;
	fbtk_widget_t *moved;
	nsfb_bbox_t cloc;

	/* ensure we have the root widget */
	root = fbtk_get_root_widget(widget);

	if (relative) {
		nsfb_cursor_loc_get(root->u.root.fb, &cloc);
		cloc.x0 += x;
		cloc.y0 += y;
	} else {
		cloc.x0 = x;
		cloc.y0 = y;
	}

	/* ensure cursor location lies within the root widget */
	if (cloc.x0 < root->x)
		cloc.x0 = root->x;
	if (cloc.x0 >= (root->x + root->width))
		cloc.x0 = (root->x + root->width) - 1;
	if (cloc.y0 < root->y)
		cloc.y0 = root->y;
	if (cloc.y0 >= (root->y + root->height))
		cloc.y0 = (root->y + root->height) - 1;

	if (root->u.root.grabbed == NULL) {
		/* update the pointer cursor */
		nsfb_cursor_loc_set(root->u.root.fb, &cloc);

		moved = fbtk_get_widget_at(root, cloc.x0, cloc.y0);

		x = fbtk_get_absx(moved);
		y = fbtk_get_absy(moved);

		/* post enter and leaving messages */
		if (moved != root->u.root.prev) {
			fbtk_post_callback(root->u.root.prev, FBTK_CBT_POINTERLEAVE);
			root->u.root.prev = moved;
			fbtk_post_callback(root->u.root.prev, FBTK_CBT_POINTERENTER);
		}
	} else {
		/* pointer movement has been grabbed by a widget */
		moved = root->u.root.grabbed;

		/* ensure pointer remains within widget boundary */
		x = fbtk_get_absx(moved);
		y = fbtk_get_absy(moved);

		if (cloc.x0 < x)
			cloc.x0 = x;
		if (cloc.y0 < y)
			cloc.y0 = y;
		if (cloc.x0 > (x + moved->width))
			cloc.x0 = (x + moved->width);
		if (cloc.y0 > (y + moved->height))
			cloc.y0 = (y + moved->height);

		/* update the pointer cursor */
		nsfb_cursor_loc_set(root->u.root.fb, &cloc);
	}

	/* post the movement */
	fbtk_post_callback(moved, FBTK_CBT_POINTERMOVE, cloc.x0 - x, cloc.y0 - y);

}

/* exported function documented in fbtk.h */
bool
fbtk_event(fbtk_widget_t *root, nsfb_event_t *event, int timeout)
{
	nsfb_bbox_t cloc;
	bool unused = false; /* is the event available */
	bool move_pointer = false; /* whether pointer move events occured */

	/* ensure we have the root widget */
	root = fbtk_get_root_widget(root);

	do {
		if (nsfb_event(root->u.root.fb, event, timeout) == false) {
			if (move_pointer)
				fbtk_warp_pointer(root, cloc.x0, cloc.y0,
						false);
			return false;
		}

		if (move_pointer && event->type != NSFB_EVENT_MOVE_RELATIVE &&
				event->type != NSFB_EVENT_MOVE_ABSOLUTE) {
			/* Flush the movements */
			fbtk_warp_pointer(root, cloc.x0, cloc.y0, false);

		} else if (!move_pointer &&
				event->type == NSFB_EVENT_MOVE_RELATIVE) {
			/* Get current pointer coords */
			nsfb_cursor_loc_get(root->u.root.fb, &cloc);
		}

		switch (event->type) {
		case NSFB_EVENT_KEY_DOWN:
		case NSFB_EVENT_KEY_UP:
			if ((event->value.keycode >= NSFB_KEY_MOUSE_1) &&
			    (event->value.keycode <= NSFB_KEY_MOUSE_5)) {
				fbtk_click(root, event);
			} else {
				fbtk_input(root, event);
			}
			break;

		case NSFB_EVENT_CONTROL:
			unused = true;
			break;

		case NSFB_EVENT_MOVE_RELATIVE:
			/* Consecutive move events are consolidated into a
			 * single pointer warp */
			move_pointer = true;
			cloc.x0 += event->value.vector.x;
			cloc.y0 += event->value.vector.y;
			timeout = 0;
			break;

		case NSFB_EVENT_MOVE_ABSOLUTE:
			/* Consecutive move events are consolidated into a
			 * single pointer warp */
			move_pointer = true;
			cloc.x0 = event->value.vector.x;
			cloc.y0 = event->value.vector.y;
			timeout = 0;
			break;

		default:
			break;
		}
	} while (event->type == NSFB_EVENT_MOVE_RELATIVE ||
			event->type == NSFB_EVENT_MOVE_ABSOLUTE);
	return unused;
}

static int keymap[] = {
	/* 0    1    2    3    4    5    6    7    8    9               */
	-1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,   8,   9, /*   0 -   9 */
	-1,  -1,  -1,  13,  -1,  -1,  -1,  -1,  -1,  -1, /*  10 -  19 */
	-1,  -1,  -1,  -1,  -1,  -1,  -1,  27,  -1,  -1, /*  20 -  29 */
	-1,  -1, ' ', '!', '"', '#', '$',  -1, '&','\'', /*  30 -  39 */
	'(', ')', '*', '+', ',', '-', '.', '/', '0', '1', /*  40 -  49 */
	'2', '3', '4', '5', '6', '7', '8', '9', ':', ';', /*  50 -  59 */
	'<', '=', '>', '?', '@',  -1,  -1,  -1,  -1,  -1, /*  60 -  69 */
	-1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*  70 -  79 */
	-1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*  80 -  89 */
	-1, '[','\\', ']', '~', '_', '`', 'a', 'b', 'c', /*  90 -  99 */
	'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', /* 100 - 109 */
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', /* 110 - 119 */
	'x', 'y', 'z',  -1,  -1,  -1,  -1,  -1,  -1,  -1, /* 120 - 129 */
};

static int sh_keymap[] = {
	/* 0    1    2    3    4    5    6    7    8    9               */
	-1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,   8,   9, /*   0 -   9 */
	-1,  -1,  -1,  13,  -1,  -1,  -1,  -1,  -1,  -1, /*  10 -  19 */
	-1,  -1,  -1,  -1,  -1,  -1,  -1,  27,  -1,  -1, /*  20 -  29 */
	-1,  -1, ' ', '!', '"', '~', '$',  -1, '&', '@', /*  30 -  39 */
	'(', ')', '*', '+', '<', '_', '>', '?', ')', '!', /*  40 -  49 */
	'"', 243, '$', '%', '^', '&', '*', '(', ';', ':', /*  50 -  59 */
	'<', '+', '>', '?', '@',  -1,  -1,  -1,  -1,  -1, /*  60 -  69 */
	-1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*  70 -  79 */
	-1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*  80 -  89 */
	-1, '{', '|', '}', '~', '_', 254, 'A', 'B', 'C', /*  90 -  99 */
	'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', /* 100 - 109 */
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', /* 110 - 119 */
	'X', 'Y', 'Z',  -1,  -1,  -1,  -1,  -1,  -1,  -1, /* 120 - 129 */
};


/* exported function documented in fbtk.h */
int
fbtk_keycode_to_ucs4(int code, fbtk_modifier_type mods)
{
	int ucs4 = -1;

	if (mods & FBTK_MOD_LSHIFT || mods & FBTK_MOD_RSHIFT) {
		if ((code >= 0) && (code < (int) NOF_ELEMENTS(sh_keymap)))
			ucs4 = sh_keymap[code];

	} else if (mods == FBTK_MOD_CLEAR) {
		if ((code >= 0) && (code < (int) NOF_ELEMENTS(keymap)))
			ucs4 = keymap[code];

	} else if (mods & FBTK_MOD_LCTRL || mods & FBTK_MOD_RCTRL) {
		switch (code) {
		case NSFB_KEY_a:
			ucs4 = KEY_SELECT_ALL;
			break;

		case NSFB_KEY_c:
			ucs4 = KEY_COPY_SELECTION;
			break;

		case NSFB_KEY_u:
			ucs4 = KEY_DELETE_LINE;
			break;

		case NSFB_KEY_v:
			ucs4 = KEY_PASTE;
			break;

		case NSFB_KEY_x:
			ucs4 = KEY_CUT_SELECTION;
			break;

		case NSFB_KEY_z:
			ucs4 = KEY_CLEAR_SELECTION;
			break;
		default:
			break;
		}
	}
	return ucs4;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
