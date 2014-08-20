/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>  
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

/** \file
 * User interaction with a CONTENT_HTML (implementation).
 */

#include <assert.h>
#include <stdbool.h>

#include <dom/dom.h>

#include "content/content.h"
#include "content/hlcache.h"
#include "desktop/browser.h"
#include "desktop/gui_factory.h"
#include "desktop/frames.h"
#include "desktop/mouse.h"
#include "utils/nsoption.h"
#include "desktop/scrollbar.h"
#include "desktop/selection.h"
#include "desktop/textarea.h"
#include "desktop/textinput.h"
#include "render/box.h"
#include "render/box_textarea.h"
#include "render/font.h"
#include "render/form.h"
#include "render/html_internal.h"
#include "render/imagemap.h"
#include "render/search.h"
#include "javascript/js.h"
#include "utils/corestrings.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/log.h"


/**
 * Get pointer shape for given box
 *
 * \param box       box in question
 * \param imagemap  whether an imagemap applies to the box
 */

static browser_pointer_shape get_pointer_shape(struct box *box, bool imagemap)
{
	browser_pointer_shape pointer;
	css_computed_style *style;
	enum css_cursor_e cursor;
	lwc_string **cursor_uris;

	if (box->type == BOX_FLOAT_LEFT || box->type == BOX_FLOAT_RIGHT)
		style = box->children->style;
	else
		style = box->style;

	if (style == NULL)
		return BROWSER_POINTER_DEFAULT;

	cursor = css_computed_cursor(style, &cursor_uris);

	switch (cursor) {
	case CSS_CURSOR_AUTO:
		if (box->href || (box->gadget &&
				(box->gadget->type == GADGET_IMAGE ||
				box->gadget->type == GADGET_SUBMIT)) ||
				imagemap) {
			/* link */
			pointer = BROWSER_POINTER_POINT;
		} else if (box->gadget &&
				(box->gadget->type == GADGET_TEXTBOX ||
				box->gadget->type == GADGET_PASSWORD ||
				box->gadget->type == GADGET_TEXTAREA)) {
			/* text input */
			pointer = BROWSER_POINTER_CARET;
		} else {
			/* html content doesn't mind */
			pointer = BROWSER_POINTER_AUTO;
		}
		break;
	case CSS_CURSOR_CROSSHAIR:
		pointer = BROWSER_POINTER_CROSS;
		break;
	case CSS_CURSOR_POINTER:
		pointer = BROWSER_POINTER_POINT;
		break;
	case CSS_CURSOR_MOVE:
		pointer = BROWSER_POINTER_MOVE;
		break;
	case CSS_CURSOR_E_RESIZE:
		pointer = BROWSER_POINTER_RIGHT;
		break;
	case CSS_CURSOR_W_RESIZE:
		pointer = BROWSER_POINTER_LEFT;
		break;
	case CSS_CURSOR_N_RESIZE:
		pointer = BROWSER_POINTER_UP;
		break;
	case CSS_CURSOR_S_RESIZE:
		pointer = BROWSER_POINTER_DOWN;
		break;
	case CSS_CURSOR_NE_RESIZE:
		pointer = BROWSER_POINTER_RU;
		break;
	case CSS_CURSOR_SW_RESIZE:
		pointer = BROWSER_POINTER_LD;
		break;
	case CSS_CURSOR_SE_RESIZE:
		pointer = BROWSER_POINTER_RD;
		break;
	case CSS_CURSOR_NW_RESIZE:
		pointer = BROWSER_POINTER_LU;
		break;
	case CSS_CURSOR_TEXT:
		pointer = BROWSER_POINTER_CARET;
		break;
	case CSS_CURSOR_WAIT:
		pointer = BROWSER_POINTER_WAIT;
		break;
	case CSS_CURSOR_PROGRESS:
		pointer = BROWSER_POINTER_PROGRESS;
		break;
	case CSS_CURSOR_HELP:
		pointer = BROWSER_POINTER_HELP;
		break;
	default:
		pointer = BROWSER_POINTER_DEFAULT;
		break;
	}

	return pointer;
}


/**
 * Start drag scrolling the contents of a box
 *
 * \param box	the box to be scrolled
 * \param x	x ordinate of initial mouse position
 * \param y	y ordinate
 */

static void html_box_drag_start(struct box *box, int x, int y)
{
	int box_x, box_y;
	int scroll_mouse_x, scroll_mouse_y;

	box_coords(box, &box_x, &box_y);

	if (box->scroll_x != NULL) {
		scroll_mouse_x = x - box_x ;
		scroll_mouse_y = y - (box_y + box->padding[TOP] +
				box->height + box->padding[BOTTOM] -
				SCROLLBAR_WIDTH);
		scrollbar_start_content_drag(box->scroll_x,
				scroll_mouse_x, scroll_mouse_y);
	} else if (box->scroll_y != NULL) {
		scroll_mouse_x = x - (box_x + box->padding[LEFT] +
				box->width + box->padding[RIGHT] -
				SCROLLBAR_WIDTH);
		scroll_mouse_y = y - box_y;

		scrollbar_start_content_drag(box->scroll_y,
				scroll_mouse_x, scroll_mouse_y);
	}
}


/**
 * End overflow scroll scrollbar drags
 *
 * \param  h      html content's high level cache entry
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */
static size_t html_selection_drag_end(struct html_content *html,
		browser_mouse_state mouse, int x, int y, int dir)
{
	int pixel_offset;
	struct box *box;
	int dx, dy;
	size_t idx = 0;

	box = box_pick_text_box(html, x, y, dir, &dx, &dy);
	if (box) {
		plot_font_style_t fstyle;

		font_plot_style_from_css(box->style, &fstyle);

		nsfont.font_position_in_string(&fstyle, box->text, box->length,
				dx, &idx, &pixel_offset);

		idx += box->byte_offset;
	}

	return idx;
}


/**
 * Handle mouse tracking (including drags) in an HTML content window.
 *
 * \param  c	  content of type html
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */

void html_mouse_track(struct content *c, struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	html_mouse_action(c, bw, mouse, x, y);
}

/** Helper for file gadgets to store their filename unencoded on the
 * dom node associated with the gadget.
 *
 * \todo Get rid of this crap eventually
 */
static void html__image_coords_dom_user_data_handler(dom_node_operation operation,
		dom_string *key, void *_data, struct dom_node *src,
		struct dom_node *dst)
{
	struct image_input_coords *oldcoords, *coords = _data, *newcoords;

	if (!dom_string_isequal(corestring_dom___ns_key_image_coords_node_data,
				key) || coords == NULL) {
		return;
	}

	switch (operation) {
	case DOM_NODE_CLONED:
		newcoords = calloc(1, sizeof(*newcoords));
		*newcoords = *coords;
		if (dom_node_set_user_data(dst,
					   corestring_dom___ns_key_image_coords_node_data,
					   newcoords, html__image_coords_dom_user_data_handler,
					   &oldcoords) == DOM_NO_ERR) {
			free(oldcoords);
		}
		break;

	case DOM_NODE_RENAMED:
	case DOM_NODE_IMPORTED:
	case DOM_NODE_ADOPTED:
		break;

	case DOM_NODE_DELETED:
		free(coords);
		break;
	default:
		LOG(("User data operation not handled."));
		assert(0);
	}
}

/**
 * Handle mouse clicks and movements in an HTML content window.
 *
 * \param  c	  content of type html
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 *
 * This function handles both hovering and clicking. It is important that the
 * code path is identical (except that hovering doesn't carry out the action),
 * so that the status bar reflects exactly what will happen. Having separate
 * code paths opens the possibility that an attacker will make the status bar
 * show some harmless action where clicking will be harmful.
 */

void html_mouse_action(struct content *c, struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	html_content *html = (html_content *) c;
	enum { ACTION_NONE, ACTION_SUBMIT, ACTION_GO } action = ACTION_NONE;
	const char *title = 0;
	nsurl *url = 0;
	const char *target = 0;
	char status_buffer[200];
	const char *status = 0;
	browser_pointer_shape pointer = BROWSER_POINTER_DEFAULT;
	bool imagemap = false;
	int box_x = 0, box_y = 0;
	int gadget_box_x = 0, gadget_box_y = 0;
	int html_object_pos_x = 0, html_object_pos_y = 0;
	int text_box_x = 0;
	struct box *url_box = 0;
	struct box *gadget_box = 0;
	struct box *text_box = 0;
	struct box *box;
	struct form_control *gadget = 0;
	hlcache_handle *object = NULL;
	struct box *html_object_box = NULL;
	struct browser_window *iframe = NULL;
	struct box *next_box;
	struct box *drag_candidate = NULL;
	struct scrollbar *scrollbar = NULL;
	plot_font_style_t fstyle;
	int scroll_mouse_x = 0, scroll_mouse_y = 0;
	int padding_left, padding_right, padding_top, padding_bottom;
	browser_drag_type drag_type = browser_window_get_drag_type(bw);
	union content_msg_data msg_data;
	struct dom_node *node = NULL;
	union html_drag_owner drag_owner;
	union html_selection_owner sel_owner;
	bool click = mouse & (BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_PRESS_2 |
			BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_CLICK_2 |
			BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_2);

	if (drag_type != DRAGGING_NONE && !mouse &&
			html->visible_select_menu != NULL) {
		/* drag end: select menu */
		form_select_mouse_drag_end(html->visible_select_menu,
				mouse, x, y);
	}

	if (html->visible_select_menu != NULL) {
		box = html->visible_select_menu->box;
		box_coords(box, &box_x, &box_y);

		box_x -= box->border[LEFT].width;
		box_y += box->height + box->border[BOTTOM].width +
				box->padding[BOTTOM] + box->padding[TOP];
		status = form_select_mouse_action(html->visible_select_menu,
				mouse, x - box_x, y - box_y);
		if (status != NULL) {
			msg_data.explicit_status_text = status;
			content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
		} else {
			int width, height;
			form_select_get_dimensions(html->visible_select_menu,
					&width, &height);
			html->visible_select_menu = NULL;
			browser_window_redraw_rect(bw, box_x, box_y,
					width, height);
		}
		return;
	}

	if (html->drag_type == HTML_DRAG_SELECTION) {
		/* Selection drag */
		struct box *box;
		int dir = -1;
		int dx, dy;

		if (!mouse) {
			/* End of selection drag */
			int dir = -1;
			size_t idx;

			if (selection_dragging_start(&html->sel))
				dir = 1;

			idx = html_selection_drag_end(html, mouse, x, y, dir);

			if (idx != 0)
				selection_track(&html->sel, mouse, idx);

			drag_owner.no_owner = true;
			html_set_drag_type(html, HTML_DRAG_NONE,
					drag_owner, NULL);
			return;
		}

		if (selection_dragging_start(&html->sel))
			dir = 1;

		box = box_pick_text_box(html, x, y, dir, &dx, &dy);

		if (box != NULL) {
			int pixel_offset;
			size_t idx;
			plot_font_style_t fstyle;

			font_plot_style_from_css(box->style, &fstyle);

			nsfont.font_position_in_string(&fstyle,
					box->text, box->length,
					dx, &idx, &pixel_offset);

			selection_track(&html->sel, mouse,
					box->byte_offset + idx);
		}
		return;
	}

	if (html->drag_type == HTML_DRAG_SCROLLBAR) {
		struct scrollbar *scr = html->drag_owner.scrollbar;
		struct html_scrollbar_data *data = scrollbar_get_data(scr);

		if (!mouse) {
			/* drag end: scrollbar */
			html_overflow_scroll_drag_end(scr, mouse, x, y);
		}

		box = data->box;
		box_coords(box, &box_x, &box_y);
		if (scrollbar_is_horizontal(scr)) {
			scroll_mouse_x = x - box_x ;
			scroll_mouse_y = y - (box_y + box->padding[TOP] +
					box->height + box->padding[BOTTOM] -
					SCROLLBAR_WIDTH);
			status = scrollbar_mouse_status_to_message(
					scrollbar_mouse_action(scr, mouse,
							scroll_mouse_x,
							scroll_mouse_y));
		} else {
			scroll_mouse_x = x - (box_x + box->padding[LEFT] +
					box->width + box->padding[RIGHT] -
					SCROLLBAR_WIDTH);
			scroll_mouse_y = y - box_y;
			status = scrollbar_mouse_status_to_message(
					scrollbar_mouse_action(scr, mouse,
							scroll_mouse_x,
							scroll_mouse_y));
		}

		msg_data.explicit_status_text = status;
		content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
		return;
	}

	if (html->drag_type == HTML_DRAG_TEXTAREA_SELECTION ||
			html->drag_type == HTML_DRAG_TEXTAREA_SCROLLBAR) {
		box = html->drag_owner.textarea;
		assert(box->gadget != NULL);
		assert(box->gadget->type == GADGET_TEXTAREA ||
				box->gadget->type == GADGET_PASSWORD ||
				box->gadget->type == GADGET_TEXTBOX);

		box_coords(box, &box_x, &box_y);
		textarea_mouse_action(box->gadget->data.text.ta, mouse,
				x - box_x, y - box_y);

		/* TODO: Set appropriate statusbar message */
		return;
	}

	if (html->drag_type == HTML_DRAG_CONTENT_SELECTION ||
			html->drag_type == HTML_DRAG_CONTENT_SCROLL) {
		box = html->drag_owner.content;
		assert(box->object != NULL);

		box_coords(box, &box_x, &box_y);
		content_mouse_track(box->object, bw, mouse,
				x - box_x, y - box_y);
		return;
	}

	if (html->drag_type == HTML_DRAG_CONTENT_SELECTION) {
		box = html->drag_owner.content;
		assert(box->object != NULL);

		box_coords(box, &box_x, &box_y);
		content_mouse_track(box->object, bw, mouse,
				x - box_x, y - box_y);
		return;
	}

	/* Content related drags handled by now */
	assert(html->drag_type == HTML_DRAG_NONE);

	/* search the box tree for a link, imagemap, form control, or
	 * box with scrollbars 
	 */

	box = html->layout;

	/* Consider the margins of the html page now */
	box_x = box->margin[LEFT];
	box_y = box->margin[TOP];

	/* descend through visible boxes setting more specific values for:
	 * box - deepest box at point 
	 * html_object_box - html object
	 * html_object_pos_x - html object
	 * html_object_pos_y - html object
	 * object - non html object
	 * iframe - iframe
	 * url - href or imagemap
	 * target - href or imagemap or gadget
	 * url_box - href or imagemap
	 * imagemap - imagemap
	 * gadget - gadget
	 * gadget_box - gadget
	 * gadget_box_x - gadget
	 * gadget_box_y - gadget
	 * title - title
	 * pointer
	 *
	 * drag_candidate - first box with scroll
	 * padding_left - box with scroll
	 * padding_right
	 * padding_top
	 * padding_bottom
	 * scrollbar - inside padding box stops decent
	 * scroll_mouse_x - inside padding box stops decent
	 * scroll_mouse_y - inside padding box stops decent
	 * 
	 * text_box - text box
	 * text_box_x - text_box
	 */
	while ((next_box = box_at_point(box, x, y, &box_x, &box_y)) != NULL) {
		box = next_box;

		if ((box->style != NULL) && 
		    (css_computed_visibility(box->style) == 
		     CSS_VISIBILITY_HIDDEN)) {
			continue;
		}

		if (box->node != NULL) {
			node = box->node;
		}

		if (box->object) {
			if (content_get_type(box->object) == CONTENT_HTML) {
				html_object_box = box;
				html_object_pos_x = box_x;
				html_object_pos_y = box_y;
			} else {
				object = box->object;
			}
		}

		if (box->iframe) {
			iframe = box->iframe;
		}

		if (box->href) {
			url = box->href;
			target = box->target;
			url_box = box;
		}

		if (box->usemap) {
			url = imagemap_get(html, box->usemap,
					box_x, box_y, x, y, &target);
			if (url) {
				imagemap = true;
				url_box = box;
			}
		}

		if (box->gadget) {
			gadget = box->gadget;
			gadget_box = box;
			gadget_box_x = box_x;
			gadget_box_y = box_y;
			if (gadget->form)
				target = gadget->form->target;
		}

		if (box->title) {
			title = box->title;
		}

		pointer = get_pointer_shape(box, false);
		
		if ((box->scroll_x != NULL) || 
		    (box->scroll_y != NULL)) {

			if (drag_candidate == NULL) {
				drag_candidate = box;
			}

			padding_left = box_x +
					scrollbar_get_offset(box->scroll_x);
			padding_right = padding_left + box->padding[LEFT] +
					box->width + box->padding[RIGHT];
			padding_top = box_y +
					scrollbar_get_offset(box->scroll_y);
			padding_bottom = padding_top + box->padding[TOP] +
					box->height + box->padding[BOTTOM];
			
			if ((x > padding_left) && 
			    (x < padding_right) &&
			    (y > padding_top) && 
			    (y < padding_bottom)) {
				/* mouse inside padding box */
				
				if ((box->scroll_y != NULL) && 
						(x > (padding_right -
							SCROLLBAR_WIDTH))) {
					/* mouse above vertical box scroll */
					
					scrollbar = box->scroll_y;
					scroll_mouse_x = x - (padding_right -
							     SCROLLBAR_WIDTH);
					scroll_mouse_y = y - padding_top;
					break;
				
				} else if ((box->scroll_x != NULL) &&
						(y > (padding_bottom -
					   		SCROLLBAR_WIDTH))) {
					/* mouse above horizontal box scroll */
							
					scrollbar = box->scroll_x;
					scroll_mouse_x = x - padding_left;
					scroll_mouse_y = y - (padding_bottom -
							SCROLLBAR_WIDTH);
					break;
				}
			}
		}

		if (box->text && !box->object) {
			text_box = box;
			text_box_x = box_x;
		}
	}

	/* use of box_x, box_y, or content below this point is probably a
	 * mistake; they will refer to the last box returned by box_at_point */

	if (scrollbar) {
		status = scrollbar_mouse_status_to_message(
				scrollbar_mouse_action(scrollbar, mouse,
						scroll_mouse_x,
						scroll_mouse_y));
		pointer = BROWSER_POINTER_DEFAULT;
	} else if (gadget) {
		textarea_mouse_status ta_status;

		switch (gadget->type) {
		case GADGET_SELECT:
			status = messages_get("FormSelect");
			pointer = BROWSER_POINTER_MENU;
			if (mouse & BROWSER_MOUSE_CLICK_1 &&
			    nsoption_bool(core_select_menu)) {
				html->visible_select_menu = gadget;
				form_open_select_menu(c, gadget,
						form_select_menu_callback,
						c);
				pointer = BROWSER_POINTER_DEFAULT;
			} else if (mouse & BROWSER_MOUSE_CLICK_1)
				guit->browser->create_form_select_menu(bw, gadget);
			break;
		case GADGET_CHECKBOX:
			status = messages_get("FormCheckbox");
			if (mouse & BROWSER_MOUSE_CLICK_1) {
				gadget->selected = !gadget->selected;
				dom_html_input_element_set_checked(
					(dom_html_input_element *)(gadget->node),
					gadget->selected);
				html__redraw_a_box(html, gadget_box);
			}
			break;
		case GADGET_RADIO:
			status = messages_get("FormRadio");
			if (mouse & BROWSER_MOUSE_CLICK_1)
				form_radio_set(gadget);
			break;
		case GADGET_IMAGE:
			if (mouse & BROWSER_MOUSE_CLICK_1) {
				struct image_input_coords *coords, *oldcoords;
				/** \todo Find a way to not ignore errors */
				coords = calloc(1, sizeof(*coords));
				if (coords == NULL) {
					return;
				}
				coords->x = x - gadget_box_x;
				coords->y = y - gadget_box_y;
				if (dom_node_set_user_data(
					    gadget->node,
					    corestring_dom___ns_key_image_coords_node_data,
					    coords, html__image_coords_dom_user_data_handler,
					    &oldcoords) != DOM_NO_ERR)
					return;
				free(oldcoords);
			}
			/* drop through */
		case GADGET_SUBMIT:
			if (gadget->form) {
				snprintf(status_buffer, sizeof status_buffer,
						messages_get("FormSubmit"),
						gadget->form->action);
				status = status_buffer;
				pointer = get_pointer_shape(gadget_box, false);
				if (mouse & (BROWSER_MOUSE_CLICK_1 |
						BROWSER_MOUSE_CLICK_2))
					action = ACTION_SUBMIT;
			} else {
				status = messages_get("FormBadSubmit");
			}
			break;
		case GADGET_TEXTBOX:
		case GADGET_PASSWORD:
		case GADGET_TEXTAREA:
			if (gadget->type == GADGET_TEXTAREA)
				status = messages_get("FormTextarea");
			else
				status = messages_get("FormTextbox");

			if (click && (html->selection_type !=
					HTML_SELECTION_TEXTAREA ||
					html->selection_owner.textarea !=
					gadget_box)) {
				sel_owner.none = true;
				html_set_selection(html, HTML_SELECTION_NONE,
						sel_owner, true);
			}

			ta_status = textarea_mouse_action(gadget->data.text.ta,
					mouse, x - gadget_box_x,
					y - gadget_box_y);

			if (ta_status & TEXTAREA_MOUSE_EDITOR) {
				pointer = get_pointer_shape(gadget_box, false);
			} else {
				pointer = BROWSER_POINTER_DEFAULT;
				status = scrollbar_mouse_status_to_message(
						ta_status >> 3);
			}
			break;
		case GADGET_HIDDEN:
			/* not possible: no box generated */
			break;
		case GADGET_RESET:
			status = messages_get("FormReset");
			break;
		case GADGET_FILE:
			status = messages_get("FormFile");
			if (mouse & BROWSER_MOUSE_CLICK_1) {
				msg_data.gadget_click.gadget = gadget;
				content_broadcast(c, CONTENT_MSG_GADGETCLICK, msg_data);
			}
			break;
		case GADGET_BUTTON:
			/* This gadget cannot be activated */
			status = messages_get("FormButton");
			break;
		}

	} else if (object && (mouse & BROWSER_MOUSE_MOD_2)) {

		if (mouse & BROWSER_MOUSE_DRAG_2) {
			msg_data.dragsave.type = CONTENT_SAVE_NATIVE;
			msg_data.dragsave.content = object;
			content_broadcast(c, CONTENT_MSG_DRAGSAVE, msg_data);

		} else if (mouse & BROWSER_MOUSE_DRAG_1) {
			msg_data.dragsave.type = CONTENT_SAVE_ORIG;
			msg_data.dragsave.content = object;
			content_broadcast(c, CONTENT_MSG_DRAGSAVE, msg_data);
		}

		/* \todo should have a drag-saving object msg */

	} else if (iframe) {
		int pos_x, pos_y;
		float scale = browser_window_get_scale(bw);

		browser_window_get_position(iframe, false, &pos_x, &pos_y);

		pos_x /= scale;
		pos_y /= scale;

		if (mouse & BROWSER_MOUSE_CLICK_1 ||
				mouse & BROWSER_MOUSE_CLICK_2) {
			browser_window_mouse_click(iframe, mouse,
					x - pos_x, y - pos_y);
		} else {
			browser_window_mouse_track(iframe, mouse,
					x - pos_x, y - pos_y);
		}
	} else if (html_object_box) {

		if (click && (html->selection_type != HTML_SELECTION_CONTENT ||
				html->selection_owner.content !=
						html_object_box)) {
			sel_owner.none = true;
			html_set_selection(html, HTML_SELECTION_NONE,
					sel_owner, true);
		}
		if (mouse & BROWSER_MOUSE_CLICK_1 ||
				mouse & BROWSER_MOUSE_CLICK_2) {
			content_mouse_action(html_object_box->object,
					bw, mouse,
					x - html_object_pos_x,
					y - html_object_pos_y);
		} else {
			content_mouse_track(html_object_box->object,
					bw, mouse,
					x - html_object_pos_x,
					y - html_object_pos_y);
		}
	} else if (url) {
		if (title) {
			snprintf(status_buffer, sizeof status_buffer, "%s: %s",
					nsurl_access(url), title);
			status = status_buffer;
		} else
			status = nsurl_access(url);

		pointer = get_pointer_shape(url_box, imagemap);

		if (mouse & BROWSER_MOUSE_CLICK_1 &&
				mouse & BROWSER_MOUSE_MOD_1) {
			/* force download of link */
			browser_window_navigate(bw,
				url,
				content_get_url(c),
				BW_NAVIGATE_DOWNLOAD,
				NULL,
				NULL,
				NULL);

		} else if (mouse & BROWSER_MOUSE_CLICK_2 &&
				mouse & BROWSER_MOUSE_MOD_1) {
			msg_data.savelink.url = nsurl_access(url);
			msg_data.savelink.title = title;
			content_broadcast(c, CONTENT_MSG_SAVELINK, msg_data);

		} else if (mouse & (BROWSER_MOUSE_CLICK_1 |
				BROWSER_MOUSE_CLICK_2))
			action = ACTION_GO;
	} else {
		bool done = false;

		/* frame resizing */
		if (browser_window_frame_resize_start(bw, mouse, x, y,
				&pointer)) {
			if (mouse & (BROWSER_MOUSE_DRAG_1 |
					BROWSER_MOUSE_DRAG_2)) {
				status = messages_get("FrameDrag");
			}
			done = true;
		}

		/* if clicking in the main page, remove the selection from any
		 * text areas */
		if (!done) {
			
			if (click && html->focus_type != HTML_FOCUS_SELF) {
				union html_focus_owner fo;
				fo.self = true;
				html_set_focus(html, HTML_FOCUS_SELF, fo,
						true, 0, 0, 0, NULL);
			}
			if (click && html->selection_type !=
					HTML_SELECTION_SELF) {
				sel_owner.none = true;
				html_set_selection(html, HTML_SELECTION_NONE,
						sel_owner, true);
			}

			if (text_box) {
				int pixel_offset;
				size_t idx;

				font_plot_style_from_css(text_box->style,
						&fstyle);

				nsfont.font_position_in_string(&fstyle,
					text_box->text,
					text_box->length,
					x - text_box_x,
					&idx,
					&pixel_offset);

				if (selection_click(&html->sel, mouse,
						text_box->byte_offset + idx)) {
					/* key presses must be directed at the
					 * main browser window, paste text
					 * operations ignored */
					html_drag_type drag_type;
					union html_drag_owner drag_owner;

					if (selection_dragging(&html->sel)) {
						drag_type = HTML_DRAG_SELECTION;
						drag_owner.no_owner = true;
						html_set_drag_type(html,
								drag_type,
								drag_owner,
								NULL);
						status = messages_get(
								"Selecting");
					}

					done = true;
				}

			} else if (mouse & BROWSER_MOUSE_PRESS_1) {
				union html_selection_owner sel_owner;
				sel_owner.none = true;
				selection_clear(&html->sel, true);
			}

			if (selection_defined(&html->sel)) {
				sel_owner.none = false;
				html_set_selection(html, HTML_SELECTION_SELF,
						sel_owner, true);
			} else if (click && html->selection_type !=
					HTML_SELECTION_NONE) {
				sel_owner.none = true;
				html_set_selection(html, HTML_SELECTION_NONE,
						sel_owner, true);
			}
		}

		if (!done) {
			if (title)
				status = title;

			if (mouse & BROWSER_MOUSE_DRAG_1) {
				if (mouse & BROWSER_MOUSE_MOD_2) {
					msg_data.dragsave.type =
							CONTENT_SAVE_COMPLETE;
					msg_data.dragsave.content = NULL;
					content_broadcast(c,
							CONTENT_MSG_DRAGSAVE,
							msg_data);
				} else {
					if (drag_candidate == NULL) {
						browser_window_page_drag_start(
								bw, x, y);
					} else {
						html_box_drag_start(
								drag_candidate,
								x, y);
					}
					pointer = BROWSER_POINTER_MOVE;
				}
			}
			else if (mouse & BROWSER_MOUSE_DRAG_2) {
				if (mouse & BROWSER_MOUSE_MOD_2) {
					msg_data.dragsave.type =
							CONTENT_SAVE_SOURCE;
					msg_data.dragsave.content = NULL;
					content_broadcast(c,
							CONTENT_MSG_DRAGSAVE,
							msg_data);
				} else {
					if (drag_candidate == NULL) {
						browser_window_page_drag_start(
								bw, x, y);
					} else {
						html_box_drag_start(
								drag_candidate,
								x, y);
					}
					pointer = BROWSER_POINTER_MOVE;
				}
			}
		}
		if (mouse && mouse < BROWSER_MOUSE_MOD_1) {
			/* ensure key presses still act on the browser window */
			union html_focus_owner fo;
			fo.self = true;
			html_set_focus(html, HTML_FOCUS_SELF, fo,
					true, 0, 0, 0, NULL);
		}
	}

	if (!iframe && !html_object_box) {
		msg_data.explicit_status_text = status;
		content_broadcast(c, CONTENT_MSG_STATUS, msg_data);

		msg_data.pointer = pointer;
		content_broadcast(c, CONTENT_MSG_POINTER, msg_data);
	}

	/* fire dom click event */
	if ((mouse & BROWSER_MOUSE_CLICK_1) ||
	    (mouse & BROWSER_MOUSE_CLICK_2)) {
		js_fire_event(html->jscontext, "click", html->document, node);
	}

	/* deferred actions that can cause this browser_window to be destroyed
	 * and must therefore be done after set_status/pointer
	 */
	switch (action) {
	case ACTION_SUBMIT:
		form_submit(content_get_url(c),
				browser_window_find_target(bw, target, mouse),
				gadget->form, gadget);
		break;
	case ACTION_GO:
		browser_window_navigate(browser_window_find_target(bw, target, mouse),
					url,
					content_get_url(c),
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		break;
	case ACTION_NONE:
		break;
	}
}


/**
 * Handle keypresses.
 *
 * \param  c	content of type HTML
 * \param  key	The UCS4 character codepoint
 * \return true if key handled, false otherwise
 */

bool html_keypress(struct content *c, uint32_t key)
{
	html_content *html = (html_content *) c;
	struct selection *sel = &html->sel;
	struct box *box;

	switch (html->focus_type) {
	case HTML_FOCUS_CONTENT:
		box = html->focus_owner.content;
		return content_keypress(box->object, key);

	case HTML_FOCUS_TEXTAREA:
		box = html->focus_owner.textarea;
		return box_textarea_keypress(html, box, key);

	default:
		/* Deal with it below */
		break;
	}

	switch (key) {
	case KEY_COPY_SELECTION:
		selection_copy_to_clipboard(sel);
		return true;

	case KEY_CLEAR_SELECTION:
		selection_clear(sel, true);
		return true;

	case KEY_SELECT_ALL:
		selection_select_all(sel);
		return true;

	case KEY_ESCAPE:
		if (selection_defined(sel)) {
			selection_clear(sel, true);
			return true;
		}

		/* if there's no selection, leave Escape for the caller */
		return false;
	}

	return false;
}


/**
 * Handle search.
 *
 * \param  c			content of type HTML
 * \param  context		front end private data
 * \param  flags		search flags
 * \param  string		search string
 */
void html_search(struct content *c, void *context,
		search_flags_t flags, const char *string)
{
	html_content *html = (html_content *)c;

	assert(c != NULL);

	if (string != NULL && html->search_string != NULL &&
			strcmp(string, html->search_string) == 0 &&
			html->search != NULL) {
		/* Continue prev. search */
		search_step(html->search, flags, string);

	} else if (string != NULL) {
		/* New search */
		free(html->search_string);
		html->search_string = strdup(string);
		if (html->search_string == NULL)
			return;

		if (html->search != NULL) {
			search_destroy_context(html->search);
			html->search = NULL;
		}

		html->search = search_create_context(c, CONTENT_HTML, context);

		if (html->search == NULL)
			return;

		search_step(html->search, flags, string);

	} else {
		/* Clear search */
		html_search_clear(c);

		free(html->search_string);
		html->search_string = NULL;
	}
}


/**
 * Terminate a search.
 *
 * \param  c			content of type HTML
 */
void html_search_clear(struct content *c)
{
	html_content *html = (html_content *)c;

	assert(c != NULL);

	free(html->search_string);
	html->search_string = NULL;

	if (html->search != NULL) {
		search_destroy_context(html->search);
	}
	html->search = NULL;
}


/**
 * Callback for in-page scrollbars.
 */
void html_overflow_scroll_callback(void *client_data,
		struct scrollbar_msg_data *scrollbar_data)
{
	struct html_scrollbar_data *data = client_data;
	html_content *html = (html_content *)data->c;
	struct box *box = data->box;
	union content_msg_data msg_data;
	html_drag_type drag_type;
	union html_drag_owner drag_owner;
	
	switch(scrollbar_data->msg) {
	case SCROLLBAR_MSG_MOVED:
		html__redraw_a_box(html, box);
		break;
	case SCROLLBAR_MSG_SCROLL_START:
	{
		struct rect rect = {
			.x0 = scrollbar_data->x0,
			.y0 = scrollbar_data->y0,
			.x1 = scrollbar_data->x1,
			.y1 = scrollbar_data->y1
		};
		drag_type = HTML_DRAG_SCROLLBAR;
		drag_owner.scrollbar = scrollbar_data->scrollbar;
		html_set_drag_type(html, drag_type, drag_owner, &rect);
	}
		break;
	case SCROLLBAR_MSG_SCROLL_FINISHED:
		drag_type = HTML_DRAG_NONE;
		drag_owner.no_owner = true;
		html_set_drag_type(html, drag_type, drag_owner, NULL);

		msg_data.pointer = BROWSER_POINTER_AUTO;
		content_broadcast(data->c, CONTENT_MSG_POINTER, msg_data);
		break;
	}
}


/**
 * End overflow scroll scrollbar drags
 *
 * \param  scroll  scrollbar widget
 * \param  mouse   state of mouse buttons and modifier keys
 * \param  x	   coordinate of mouse
 * \param  y	   coordinate of mouse
 */
void html_overflow_scroll_drag_end(struct scrollbar *scrollbar,
		browser_mouse_state mouse, int x, int y)
{
	int scroll_mouse_x, scroll_mouse_y, box_x, box_y;
	struct html_scrollbar_data *data = scrollbar_get_data(scrollbar);
	struct box *box;

	box = data->box;
	box_coords(box, &box_x, &box_y);

	if (scrollbar_is_horizontal(scrollbar)) {
		scroll_mouse_x = x - box_x;
		scroll_mouse_y = y - (box_y + box->padding[TOP] +
				box->height + box->padding[BOTTOM] -
				SCROLLBAR_WIDTH);
		scrollbar_mouse_drag_end(scrollbar, mouse,
				scroll_mouse_x, scroll_mouse_y);
	} else {
		scroll_mouse_x = x - (box_x + box->padding[LEFT] +
				box->width + box->padding[RIGHT] -
				SCROLLBAR_WIDTH);
		scroll_mouse_y = y - box_y;
		scrollbar_mouse_drag_end(scrollbar, mouse,
				scroll_mouse_x, scroll_mouse_y);
	}
}

/* Documented in html_internal.h */
void html_set_drag_type(html_content *html, html_drag_type drag_type,
		union html_drag_owner drag_owner, const struct rect *rect)
{
	union content_msg_data msg_data;

	assert(html != NULL);

	html->drag_type = drag_type;
	html->drag_owner = drag_owner;

	switch (drag_type) {
	case HTML_DRAG_NONE:
		assert(drag_owner.no_owner == true);
		msg_data.drag.type = CONTENT_DRAG_NONE;
		break;

	case HTML_DRAG_SCROLLBAR:
	case HTML_DRAG_TEXTAREA_SCROLLBAR:
	case HTML_DRAG_CONTENT_SCROLL:
		msg_data.drag.type = CONTENT_DRAG_SCROLL;
		break;

	case HTML_DRAG_SELECTION:
		assert(drag_owner.no_owner == true);
		/* Fall through */
	case HTML_DRAG_TEXTAREA_SELECTION:
	case HTML_DRAG_CONTENT_SELECTION:
		msg_data.drag.type = CONTENT_DRAG_SELECTION;
		break;
	}
	msg_data.drag.rect = rect;

	/* Inform of the content's drag status change */
	content_broadcast((struct content *)html, CONTENT_MSG_DRAG, msg_data);
}

/* Documented in html_internal.h */
void html_set_focus(html_content *html, html_focus_type focus_type,
		union html_focus_owner focus_owner, bool hide_caret,
		int x, int y, int height, const struct rect *clip)
{
	union content_msg_data msg_data;
	int x_off = 0;
	int y_off = 0;
	struct rect cr;
	bool textarea_lost_focus = html->focus_type == HTML_FOCUS_TEXTAREA &&
			focus_type != HTML_FOCUS_TEXTAREA;

	assert(html != NULL);

	switch (focus_type) {
	case HTML_FOCUS_SELF:
		assert(focus_owner.self == true);
		if (html->focus_type == HTML_FOCUS_SELF)
			/* Don't need to tell anyone anything */
			return;
		break;

	case HTML_FOCUS_CONTENT:
		box_coords(focus_owner.content, &x_off, &y_off);
		break;

	case HTML_FOCUS_TEXTAREA:
		box_coords(focus_owner.textarea, &x_off, &y_off);
		break;
	}

	html->focus_type = focus_type;
	html->focus_owner = focus_owner;

	if (textarea_lost_focus) {
		msg_data.caret.type = CONTENT_CARET_REMOVE;
	} else if (focus_type != HTML_FOCUS_SELF && hide_caret) {
		msg_data.caret.type = CONTENT_CARET_HIDE;
	} else {
		if (clip != NULL) {
			cr = *clip;
			cr.x0 += x_off;
			cr.y0 += y_off;
			cr.x1 += x_off;
			cr.y1 += y_off;
		}

		msg_data.caret.type = CONTENT_CARET_SET_POS;
		msg_data.caret.pos.x = x + x_off;
		msg_data.caret.pos.y = y + y_off;
		msg_data.caret.pos.height = height;
		msg_data.caret.pos.clip = (clip == NULL) ? NULL : &cr;
	}

	/* Inform of the content's drag status change */
	content_broadcast((struct content *)html, CONTENT_MSG_CARET, msg_data);
}

/* Documented in html_internal.h */
void html_set_selection(html_content *html, html_selection_type selection_type,
		union html_selection_owner selection_owner, bool read_only)
{
	union content_msg_data msg_data;
	struct box *box;
	bool changed = false;
	bool same_type = html->selection_type == selection_type;

	assert(html != NULL);

	if ((selection_type == HTML_SELECTION_NONE &&
			html->selection_type != HTML_SELECTION_NONE) ||
			(selection_type != HTML_SELECTION_NONE &&
			html->selection_type == HTML_SELECTION_NONE))
		/* Existance of selection has changed, and we'll need to
		 * inform our owner */
		changed = true;

	/* Clear any existing selection */
	if (html->selection_type != HTML_SELECTION_NONE) {
		switch (html->selection_type) {
		case HTML_SELECTION_SELF:
			if (same_type)
				break;
			selection_clear(&html->sel, true);
			break;
		case HTML_SELECTION_TEXTAREA:
			if (same_type && html->selection_owner.textarea ==
					selection_owner.textarea)
				break;
			box = html->selection_owner.textarea;
			textarea_clear_selection(box->gadget->data.text.ta);
			break;
		case HTML_SELECTION_CONTENT:
			if (same_type && html->selection_owner.content ==
					selection_owner.content)
				break;
			box = html->selection_owner.content;
			content_clear_selection(box->object);
			break;
		default:
			break;
		}
	}

	html->selection_type = selection_type;
	html->selection_owner = selection_owner;

	if (!changed)
		/* Don't need to report lack of change to owner */
		return;

	/* Prepare msg */
	switch (selection_type) {
	case HTML_SELECTION_NONE:
		assert(selection_owner.none == true);
		msg_data.selection.selection = false;
		break;
	case HTML_SELECTION_SELF:
		assert(selection_owner.none == false);
		/* fall through */
	case HTML_SELECTION_TEXTAREA:
	case HTML_SELECTION_CONTENT:
		msg_data.selection.selection = true;
		break;
	default:
		break;
	}
	msg_data.selection.read_only = read_only;

	/* Inform of the content's selection status change */
	content_broadcast((struct content *)html, CONTENT_MSG_SELECTION,
			msg_data);
}
