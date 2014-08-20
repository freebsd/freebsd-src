/*
 * Copyright 2013 Vincent Sanders <vince@netsurf-browser.org>
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
 * Processing for html content object operations.
 */

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "content/hlcache.h"
#include "css/utils.h"
#include "utils/nsoption.h"
#include "desktop/scrollbar.h"
#include "desktop/gui_factory.h"
#include "utils/corestrings.h"
#include "utils/config.h"
#include "utils/log.h"

#include "render/box.h"
#include "render/html_internal.h"

/* break reference loop */
static void html_object_refresh(void *p);

/**
 * Retrieve objects used by HTML document
 *
 * \param h  Content to retrieve objects from
 * \param n  Pointer to location to receive number of objects
 * \return Pointer to list of objects
 */
struct content_html_object *html_get_objects(hlcache_handle *h, unsigned int *n)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);
	assert(n != NULL);

	*n = c->num_objects;

	return c->object_list;
}

/**
 * Handle object fetching or loading failure.
 *
 * \param  box         box containing object which failed to load
 * \param  content     document of type CONTENT_HTML
 * \param  background  the object was the background image for the box
 */

static void
html_object_failed(struct box *box, html_content *content, bool background)
{
	/* Nothing to do */
	return;
}

/**
 * Update a box whose content has completed rendering.
 */

static void
html_object_done(struct box *box,
		 hlcache_handle *object,
		 bool background)
{
	struct box *b;

	if (background) {
		box->background = object;
		return;
	}

	box->object = object;

	if (!(box->flags & REPLACE_DIM)) {
		/* invalidate parent min, max widths */
		for (b = box; b; b = b->parent)
			b->max_width = UNKNOWN_MAX_WIDTH;

		/* delete any clones of this box */
		while (box->next && (box->next->flags & CLONE)) {
			/* box_free_box(box->next); */
			box->next = box->next->next;
		}
	}
}

/**
 * Callback for hlcache_handle_retrieve() for objects.
 */

static nserror
html_object_callback(hlcache_handle *object,
		     const hlcache_event *event,
		     void *pw)
{
	struct content_html_object *o = pw;
	html_content *c = (html_content *) o->parent;
	int x, y;
	struct box *box;

	assert(c->base.status != CONTENT_STATUS_ERROR);

	box = o->box;
	if (box == NULL && event->type != CONTENT_MSG_ERROR) {
		return NSERROR_OK;
	}

	switch (event->type) {
	case CONTENT_MSG_LOADING:
		if (c->base.status != CONTENT_STATUS_LOADING && c->bw != NULL)
			content_open(object,
					c->bw, &c->base,
					box->object_params);
		break;

	case CONTENT_MSG_READY:
		if (content_can_reformat(object)) {
			/* TODO: avoid knowledge of box internals here */
			content_reformat(object, false,
					box->max_width != UNKNOWN_MAX_WIDTH ?
							box->width : 0,
					box->max_width != UNKNOWN_MAX_WIDTH ?
							box->height : 0);

			/* Adjust parent content for new object size */
			html_object_done(box, object, o->background);
			if (c->base.status == CONTENT_STATUS_READY ||
					c->base.status == CONTENT_STATUS_DONE)
				content__reformat(&c->base, false,
						c->base.available_width,
						c->base.height);
		}
		break;

	case CONTENT_MSG_DONE:
		c->base.active--;
		LOG(("%d fetches active", c->base.active));

		html_object_done(box, object, o->background);

		if (c->base.status != CONTENT_STATUS_LOADING &&
				box->flags & REPLACE_DIM) {
			union content_msg_data data;

			if (!box_visible(box))
				break;

			box_coords(box, &x, &y);

			data.redraw.x = x + box->padding[LEFT];
			data.redraw.y = y + box->padding[TOP];
			data.redraw.width = box->width;
			data.redraw.height = box->height;
			data.redraw.full_redraw = true;

			content_broadcast(&c->base, CONTENT_MSG_REDRAW, data);
		}
		break;

	case CONTENT_MSG_ERROR:
		hlcache_handle_release(object);

		o->content = NULL;

		if (box != NULL) {
			c->base.active--;
			LOG(("%d fetches active", c->base.active));

			content_add_error(&c->base, "?", 0);
			html_object_failed(box, c, o->background);
		}
		break;

	case CONTENT_MSG_STATUS:
		if (event->data.explicit_status_text == NULL) {
			/* Object content's status text updated */
			union content_msg_data data;
			data.explicit_status_text =
					content_get_status_message(object);
			html_set_status(c, data.explicit_status_text);
			content_broadcast(&c->base, CONTENT_MSG_STATUS, data);
		} else {
			/* Object content wants to set explicit message */
			content_broadcast(&c->base, CONTENT_MSG_STATUS,
					event->data);
		}
		break;

	case CONTENT_MSG_REDRAW:
		if (c->base.status != CONTENT_STATUS_LOADING) {
			union content_msg_data data = event->data;

			if (!box_visible(box))
				break;

			box_coords(box, &x, &y);

			if (object == box->background) {
				/* Redraw request is for background */
				css_fixed hpos = 0, vpos = 0;
				css_unit hunit = CSS_UNIT_PX;
				css_unit vunit = CSS_UNIT_PX;
				int width = box->padding[LEFT] + box->width +
						box->padding[RIGHT];
				int height = box->padding[TOP] + box->height +
						box->padding[BOTTOM];
				int t, h, l, w;

				/* Need to know background-position */
				css_computed_background_position(box->style,
						&hpos, &hunit, &vpos, &vunit);

				w = content_get_width(box->background);
				if (hunit == CSS_UNIT_PCT) {
					l = (width - w) * hpos / INTTOFIX(100);
				} else {
					l = FIXTOINT(nscss_len2px(hpos, hunit,
							box->style));
				}

				h = content_get_height(box->background);
				if (vunit == CSS_UNIT_PCT) {
					t = (height - h) * vpos / INTTOFIX(100);
				} else {
					t = FIXTOINT(nscss_len2px(vpos, vunit,
							box->style));
				}

				/* Redraw area depends on background-repeat */
				switch (css_computed_background_repeat(
						box->style)) {
				case CSS_BACKGROUND_REPEAT_REPEAT:
					data.redraw.x = 0;
					data.redraw.y = 0;
					data.redraw.width = box->width;
					data.redraw.height = box->height;
					break;

				case CSS_BACKGROUND_REPEAT_REPEAT_X:
					data.redraw.x = 0;
					data.redraw.y += t;
					data.redraw.width = box->width;
					break;

				case CSS_BACKGROUND_REPEAT_REPEAT_Y:
					data.redraw.x += l;
					data.redraw.y = 0;
					data.redraw.height = box->height;
					break;

				case CSS_BACKGROUND_REPEAT_NO_REPEAT:
					data.redraw.x += l;
					data.redraw.y += t;
					break;

				default:
					break;
				}

				data.redraw.object_width = box->width;
				data.redraw.object_height = box->height;

				/* Add offset to box */
				data.redraw.x += x;
				data.redraw.y += y;
				data.redraw.object_x += x;
				data.redraw.object_y += y;

				content_broadcast(&c->base,
						CONTENT_MSG_REDRAW, data);
				break;

			} else {
				/* Non-background case */
				if (hlcache_handle_get_content(object) ==
						event->data.redraw.object) {

					int w = content_get_width(object);
					int h = content_get_height(object);

					if (w != 0) {
						data.redraw.x =
							data.redraw.x *
							box->width / w;
						data.redraw.width =
							data.redraw.width *
							box->width / w;
					}

					if (h != 0) {
						data.redraw.y =
							data.redraw.y *
							box->height / h;
						data.redraw.height =
							data.redraw.height *
							box->height / h;
					}

					data.redraw.object_width = box->width;
					data.redraw.object_height = box->height;
				}

				data.redraw.x += x + box->padding[LEFT];
				data.redraw.y += y + box->padding[TOP];
				data.redraw.object_x += x + box->padding[LEFT];
				data.redraw.object_y += y + box->padding[TOP];
			}

			content_broadcast(&c->base, CONTENT_MSG_REDRAW, data);
		}
		break;

	case CONTENT_MSG_REFRESH:
		if (content_get_type(object) == CONTENT_HTML) {
			/* only for HTML objects */
			guit->browser->schedule(event->data.delay * 1000,
					html_object_refresh, o);
		}

		break;

	case CONTENT_MSG_LINK:
		/* Don't care about favicons that aren't on top level content */
		break;

	case CONTENT_MSG_GETCTX:
		*(event->data.jscontext) = NULL;
		break;

	case CONTENT_MSG_SCROLL:
		if (box->scroll_x != NULL)
			scrollbar_set(box->scroll_x, event->data.scroll.x0,
					false);
		if (box->scroll_y != NULL)
			scrollbar_set(box->scroll_y, event->data.scroll.y0,
					false);
		break;

	case CONTENT_MSG_DRAGSAVE:
	{
		union content_msg_data msg_data;
		if (event->data.dragsave.content == NULL)
			msg_data.dragsave.content = object;
		else
			msg_data.dragsave.content =
					event->data.dragsave.content;

		content_broadcast(&c->base, CONTENT_MSG_DRAGSAVE, msg_data);
	}
		break;

	case CONTENT_MSG_SAVELINK:
	case CONTENT_MSG_POINTER:
	case CONTENT_MSG_GADGETCLICK:
		/* These messages are for browser window layer.
		 * we're not interested, so pass them on. */
		content_broadcast(&c->base, event->type, event->data);
		break;

	case CONTENT_MSG_CARET:
	{
		union html_focus_owner focus_owner;
		focus_owner.content = box;

		switch (event->data.caret.type) {
		case CONTENT_CARET_REMOVE:
		case CONTENT_CARET_HIDE:
			html_set_focus(c, HTML_FOCUS_CONTENT, focus_owner,
					true, 0, 0, 0, NULL);
			break;
		case CONTENT_CARET_SET_POS:
			html_set_focus(c, HTML_FOCUS_CONTENT, focus_owner,
					false, event->data.caret.pos.x,
					event->data.caret.pos.y,
					event->data.caret.pos.height,
					event->data.caret.pos.clip);
			break;
		}
	}
		break;

	case CONTENT_MSG_DRAG:
	{
		html_drag_type drag_type = HTML_DRAG_NONE;
		union html_drag_owner drag_owner;
		drag_owner.content = box;

		switch (event->data.drag.type) {
		case CONTENT_DRAG_NONE:
			drag_type = HTML_DRAG_NONE;
			drag_owner.no_owner = true;
			break;
		case CONTENT_DRAG_SCROLL:
			drag_type = HTML_DRAG_CONTENT_SCROLL;
			break;
		case CONTENT_DRAG_SELECTION:
			drag_type = HTML_DRAG_CONTENT_SELECTION;
			break;
		}
		html_set_drag_type(c, drag_type, drag_owner,
				event->data.drag.rect);
	}
		break;

	case CONTENT_MSG_SELECTION:
	{
		html_selection_type sel_type;
		union html_selection_owner sel_owner;

		if (event->data.selection.selection) {
			sel_type = HTML_SELECTION_CONTENT;
			sel_owner.content = box;
		} else {
			sel_type = HTML_SELECTION_NONE;
			sel_owner.none = true;
		}
		html_set_selection(c, sel_type, sel_owner,
				event->data.selection.read_only);
	}
		break;

	default:
		break;
	}

	if (c->base.status == CONTENT_STATUS_READY && c->base.active == 0 &&
			(event->type == CONTENT_MSG_LOADING ||
			event->type == CONTENT_MSG_DONE ||
			event->type == CONTENT_MSG_ERROR)) {
		/* all objects have arrived */
		content__reformat(&c->base, false, c->base.available_width,
				c->base.height);
		html_set_status(c, "");
		content_set_done(&c->base);
	}

	/* If  1) the configuration option to reflow pages while objects are
	 *        fetched is set
	 *     2) an object is newly fetched & converted,
	 *     3) the box's dimensions need to change due to being replaced
	 *     4) the object's parent HTML is ready for reformat,
	 *     5) the time since the previous reformat is more than the
	 *        configured minimum time between reformats
	 * then reformat the page to display newly fetched objects */
	else if (nsoption_bool(incremental_reflow) &&
			event->type == CONTENT_MSG_DONE &&
			box != NULL && !(box->flags & REPLACE_DIM) &&
			(c->base.status == CONTENT_STATUS_READY ||
			 c->base.status == CONTENT_STATUS_DONE) &&
			(wallclock() > c->base.reformat_time)) {
		content__reformat(&c->base, false, c->base.available_width,
				c->base.height);
	}

	return NSERROR_OK;
}

/**
 * Start a fetch for an object required by a page, replacing an existing object.
 *
 * \param  object          Object to replace
 * \param  url             URL of object to fetch (copied)
 * \return  true on success, false on memory exhaustion
 */

static bool html_replace_object(struct content_html_object *object, nsurl *url)
{
	html_content *c;
	hlcache_child_context child;
	html_content *page;
	nserror error;

	assert(object != NULL);
	assert(object->box != NULL);

	c = (html_content *) object->parent;

	child.charset = c->encoding;
	child.quirks = c->base.quirks;

	if (object->content != NULL) {
		/* remove existing object */
		if (content_get_status(object->content) != CONTENT_STATUS_DONE) {
			c->base.active--;
			LOG(("%d fetches active", c->base.active));
		}

		hlcache_handle_release(object->content);
		object->content = NULL;

		object->box->object = NULL;
	}

	/* initialise fetch */
	error = hlcache_handle_retrieve(url, HLCACHE_RETRIEVE_SNIFF_TYPE,
			content_get_url(&c->base), NULL,
			html_object_callback, object, &child,
			object->permitted_types,
			&object->content);

	if (error != NSERROR_OK)
		return false;

	for (page = c; page != NULL; page = page->page) {
		page->base.active++;
		LOG(("%d fetches active", c->base.active));

		page->base.status = CONTENT_STATUS_READY;
	}

	return true;
}

/**
 * schedule callback for object refresh
 */

static void html_object_refresh(void *p)
{
	struct content_html_object *object = p;
	nsurl *refresh_url;

	assert(content_get_type(object->content) == CONTENT_HTML);

	refresh_url = content_get_refresh_url(object->content);

	/* Ignore if refresh URL has gone
	 * (may happen if fetch errored) */
	if (refresh_url == NULL)
		return;

	content_invalidate_reuse_data(object->content);

	if (!html_replace_object(object, refresh_url)) {
		/** \todo handle memory exhaustion */
	}
}

nserror html_object_open_objects(html_content *html, struct browser_window *bw)
{
	struct content_html_object *object, *next;

	for (object = html->object_list; object != NULL; object = next) {
		next = object->next;

		if (object->content == NULL || object->box == NULL)
			continue;

		if (content_get_type(object->content) == CONTENT_NONE)
			continue;

		content_open(object->content,
			     bw,
			     &html->base,
			     object->box->object_params);
	}
	return NSERROR_OK;
}

nserror html_object_abort_objects(html_content *htmlc)
{
	struct content_html_object *object;

	for (object = htmlc->object_list;
	     object != NULL;
	     object = object->next) {
		if (object->content == NULL)
			continue;

		switch (content_get_status(object->content)) {
		case CONTENT_STATUS_DONE:
			/* already loaded: do nothing */
			break;

		case CONTENT_STATUS_READY:
			hlcache_handle_abort(object->content);
			/* Active count will be updated when
			 * html_object_callback receives
			 * CONTENT_MSG_DONE from this object
			 */
			break;

		default:
			hlcache_handle_abort(object->content);
			hlcache_handle_release(object->content);
			object->content = NULL;

			htmlc->base.active--;
			LOG(("%d fetches active", htmlc->base.active));
			break;

		}
	}

	return NSERROR_OK;
}

nserror html_object_close_objects(html_content *html)
{
	struct content_html_object *object, *next;

	for (object = html->object_list; object != NULL; object = next) {
		next = object->next;

		if (object->content == NULL || object->box == NULL)
			continue;

		if (content_get_type(object->content) == CONTENT_NONE)
			continue;

		if (content_get_type(object->content) == CONTENT_HTML) {
			guit->browser->schedule(-1, html_object_refresh, object);
		}

		content_close(object->content);
	}
	return NSERROR_OK;
}

nserror html_object_free_objects(html_content *html)
{
	while (html->object_list != NULL) {
		struct content_html_object *victim = html->object_list;

		if (victim->content != NULL) {
			LOG(("object %p", victim->content));

			if (content_get_type(victim->content) == CONTENT_HTML) {
				guit->browser->schedule(-1, html_object_refresh, victim);
			}
			hlcache_handle_release(victim->content);
		}

		html->object_list = victim->next;
		free(victim);
	}
	return NSERROR_OK;
}



/* exported interface documented in render/html_internal.h */
bool html_fetch_object(html_content *c, nsurl *url, struct box *box,
		content_type permitted_types,
		int available_width, int available_height,
		bool background)
{
	struct content_html_object *object;
	hlcache_child_context child;
	nserror error;

	/* If we've already been aborted, don't bother attempting the fetch */
	if (c->aborted)
		return true;

	child.charset = c->encoding;
	child.quirks = c->base.quirks;

	object = calloc(1, sizeof(struct content_html_object));
	if (object == NULL) {
		return false;
	}

	object->parent = (struct content *) c;
	object->next = NULL;
	object->content = NULL;
	object->box = box;
	object->permitted_types = permitted_types;
	object->background = background;

	error = hlcache_handle_retrieve(url,
			HLCACHE_RETRIEVE_SNIFF_TYPE,
			content_get_url(&c->base), NULL,
			html_object_callback, object, &child,
			object->permitted_types, &object->content);
       	if (error != NSERROR_OK) {
		free(object);
		return error != NSERROR_NOMEM;
	}

	/* add to content object list */
	object->next = c->object_list;
	c->object_list = object;

	c->num_objects++;
	if (box != NULL) {
		c->base.active++;
		LOG(("%d fetches active", c->base.active));
	}

	return true;
}
