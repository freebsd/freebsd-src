/*
 * Copyright 2005-2007 James Bursa <bursa@users.sourceforge.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Content handling (implementation).
 *
 * This implementation is based on the ::handler_map array, which maps
 * ::content_type to the functions which implement that type.
 */

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "utils/config.h"
#include "content/content_protected.h"
#include "content/hlcache.h"
#include "image/bitmap.h"
#include "desktop/browser.h"
#include "utils/nsoption.h"

#include "utils/http.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#define URL_FMT_SPC "%.140s"

const char * const content_status_name[] = {
	"LOADING",
	"READY",
	"DONE",
	"ERROR"
};

static nserror content_llcache_callback(llcache_handle *llcache,
		const llcache_event *event, void *pw);
static void content_convert(struct content *c);


/**
 * Initialise a new content structure.
 *
 * \param c                 Content to initialise
 * \param handler           Content handler
 * \param imime_type        MIME type of content
 * \param params            HTTP parameters
 * \param llcache           Source data handle
 * \param fallback_charset  Fallback charset
 * \param quirks            Quirkiness of content
 * \return NSERROR_OK on success, appropriate error otherwise
 */

nserror content__init(struct content *c, const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset, 
		bool quirks)
{
	struct content_user *user_sentinel;
	nserror error;
	
	LOG(("url "URL_FMT_SPC" -> %p", 
	     nsurl_access(llcache_handle_get_url(llcache)), c));

	user_sentinel = calloc(1, sizeof(struct content_user));
	if (user_sentinel == NULL) {
		return NSERROR_NOMEM;
	}

	if (fallback_charset != NULL) {
		c->fallback_charset = strdup(fallback_charset);
		if (c->fallback_charset == NULL) {
			free(user_sentinel);
			return NSERROR_NOMEM;
		}
	}

	c->llcache = llcache;
	c->mime_type = lwc_string_ref(imime_type);
	c->handler = handler;
	c->status = CONTENT_STATUS_LOADING;
	c->width = 0;
	c->height = 0;
	c->available_width = 0;
	c->quirks = quirks;
	c->refresh = 0;
	c->time = wallclock();
	c->size = 0;
	c->title = NULL;
	c->active = 0;
	user_sentinel->callback = NULL;
	user_sentinel->pw = NULL;
	user_sentinel->next = NULL;
	c->user_list = user_sentinel;
	c->sub_status[0] = 0;
	c->locked = false;
	c->total_size = 0;
	c->http_code = 0;
	c->error_count = 0;

	content_set_status(c, messages_get("Loading"));

	/* Finally, claim low-level cache events */
	error = llcache_handle_change_callback(llcache, 
			content_llcache_callback, c);
	if (error != NSERROR_OK) {
		lwc_string_unref(c->mime_type);
		return error;
	}

	return NSERROR_OK;
}

/**
 * Handler for low-level cache events
 *
 * \param llcache  Low-level cache handle
 * \param event	   Event details
 * \param pw	   Pointer to our context
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror content_llcache_callback(llcache_handle *llcache,
		const llcache_event *event, void *pw)
{
	struct content *c = pw;
	union content_msg_data msg_data;
	nserror error = NSERROR_OK;

	switch (event->type) {
	case LLCACHE_EVENT_HAD_HEADERS:
		/* Will never happen: handled in hlcache */
		break;
	case LLCACHE_EVENT_HAD_DATA:
		if (c->handler->process_data != NULL) {
			if (c->handler->process_data(c, 
					(const char *) event->data.data.buf, 
					event->data.data.len) == false) {
				llcache_handle_abort(c->llcache);
				c->status = CONTENT_STATUS_ERROR;
				/** \todo It's not clear what error this is */
				error = NSERROR_NOMEM;
			}
		}
		break;
	case LLCACHE_EVENT_DONE:
	{
		size_t source_size;

		(void) llcache_handle_get_source_data(llcache, &source_size);

		content_set_status(c, messages_get("Processing"));
		msg_data.explicit_status_text = NULL;
		content_broadcast(c, CONTENT_MSG_STATUS, msg_data);

		content_convert(c);
	}
		break;
	case LLCACHE_EVENT_ERROR:
		/** \todo Error page? */
		c->status = CONTENT_STATUS_ERROR;
		msg_data.error = event->data.msg;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		break;
	case LLCACHE_EVENT_PROGRESS:
		content_set_status(c, event->data.msg);
		msg_data.explicit_status_text = NULL;
		content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
		break;
	case LLCACHE_EVENT_REDIRECT:
		msg_data.redirect.from = event->data.redirect.from;
		msg_data.redirect.to = event->data.redirect.to;
		content_broadcast(c, CONTENT_MSG_REDIRECT, msg_data);
		break;
	}

	return error;
}

/**
 * Get whether a content can reformat
 *
 * \param h  content to check
 * \return whether the content can reformat
 */
bool content_can_reformat(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);

	if (c == NULL)
		return false;

	return (c->handler->reformat != NULL);
}


static void content_update_status(struct content *c)
{
	if (c->status == CONTENT_STATUS_LOADING ||
			c->status == CONTENT_STATUS_READY) {
		/* Not done yet */
		snprintf(c->status_message, sizeof (c->status_message),
				"%s%s%s", messages_get("Fetching"),
				c->sub_status[0] != '\0' ? ", " : " ",
				c->sub_status);
	} else {
		unsigned int time = c->time;
		snprintf(c->status_message, sizeof (c->status_message),
				"%s (%.1fs)", messages_get("Done"),
				(float) time / 100);
	}
}


/**
 * Updates content with new status.
 *
 * The textual status contained in the content is updated with given string.
 *
 * \param status_message  new textual status
 */

void content_set_status(struct content *c, const char *status_message)
{
	size_t len = strlen(status_message);

	if (len >= sizeof(c->sub_status)) {
		len = sizeof(c->sub_status) - 1;
	}
	memcpy(c->sub_status, status_message, len);
	c->sub_status[len] = '\0';

	content_update_status(c);
}


/**
 * All data has arrived, convert for display.
 *
 * Calls the convert function for the content.
 *
 * - If the conversion succeeds, but there is still some processing required
 *   (eg. loading images), the content gets status CONTENT_STATUS_READY, and a
 *   CONTENT_MSG_READY is sent to all users.
 * - If the conversion succeeds and is complete, the content gets status
 *   CONTENT_STATUS_DONE, and CONTENT_MSG_READY then CONTENT_MSG_DONE are sent.
 * - If the conversion fails, CONTENT_MSG_ERROR is sent. The content will soon
 *   be destroyed and must no longer be used.
 */

void content_convert(struct content *c)
{
	assert(c);
	assert(c->status == CONTENT_STATUS_LOADING ||
			c->status == CONTENT_STATUS_ERROR);

	if (c->status != CONTENT_STATUS_LOADING)
		return;

	if (c->locked == true)
		return;
	
	LOG(("content "URL_FMT_SPC" (%p)",
			nsurl_access(llcache_handle_get_url(c->llcache)), c));

	if (c->handler->data_complete != NULL) {
		c->locked = true;
		if (c->handler->data_complete(c) == false) {
			content_set_error(c);
		}
		/* Conversion to the READY state will unlock the content */
	} else {
		content_set_ready(c);
		content_set_done(c);
	}
}

/**
 * Put a content in status CONTENT_STATUS_READY and unlock the content.
 */

void content_set_ready(struct content *c)
{
	union content_msg_data msg_data;

	/* The content must be locked at this point, as it can only 
	 * become READY after conversion. */
	assert(c->locked);
	c->locked = false;

	c->status = CONTENT_STATUS_READY;
	content_update_status(c);
	content_broadcast(c, CONTENT_MSG_READY, msg_data);
}

/**
 * Put a content in status CONTENT_STATUS_DONE.
 */

void content_set_done(struct content *c)
{
	union content_msg_data msg_data;

	c->status = CONTENT_STATUS_DONE;
	c->time = wallclock() - c->time;
	content_update_status(c);
	content_broadcast(c, CONTENT_MSG_DONE, msg_data);
}

/**
 * Put a content in status CONTENT_STATUS_ERROR and unlock the content.
 *
 * \note We expect the caller to broadcast an error report if needed.
 */

void content_set_error(struct content *c)
{
	c->locked = false;
	c->status = CONTENT_STATUS_ERROR;
}

/**
 * Reformat to new size.
 *
 * Calls the reformat function for the content.
 */

void content_reformat(hlcache_handle *h, bool background,
		int width, int height)
{
	content__reformat(hlcache_handle_get_content(h), background,
			width, height);
}

void content__reformat(struct content *c, bool background,
		int width, int height)
{
	union content_msg_data data;
	assert(c != 0);
	assert(c->status == CONTENT_STATUS_READY ||
			c->status == CONTENT_STATUS_DONE);
	assert(c->locked == false);
	LOG(("%p %s", c, nsurl_access(llcache_handle_get_url(c->llcache))));
	c->available_width = width;
	if (c->handler->reformat != NULL) {

		c->locked = true;
		c->handler->reformat(c, width, height);
		c->locked = false;

		data.background = background;
		content_broadcast(c, CONTENT_MSG_REFORMAT, data);
	}
}


/**
 * Destroy and free a content.
 *
 * Calls the destroy function for the content, and frees the structure.
 */

void content_destroy(struct content *c)
{
	struct content_rfc5988_link *link;

	assert(c);
	LOG(("content %p %s", c,
			nsurl_access(llcache_handle_get_url(c->llcache))));
	assert(c->locked == false);

	if (c->handler->destroy != NULL)
		c->handler->destroy(c);

	llcache_handle_release(c->llcache);
	c->llcache = NULL;

	lwc_string_unref(c->mime_type);

	/* release metadata links */
	link = c->links;
	while (link != NULL) {
		link = content__free_rfc5988_link(link);
	}

	/* free the user list */
	if (c->user_list != NULL) {
		free(c->user_list);
	}

	/* free the title */
	if (c->title != NULL) {
		free(c->title);
	}

	/* free the fallback characterset */
	if (c->fallback_charset != NULL) {
		free(c->fallback_charset);
	}

	free(c);
}


/**
 * Handle mouse movements in a content window.
 *
 * \param  h	  Content handle
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */

void content_mouse_track(hlcache_handle *h, struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != NULL);

	if (c->handler->mouse_track != NULL) {
		c->handler->mouse_track(c, bw, mouse, x, y);
	} else {
		union content_msg_data msg_data;
		msg_data.pointer = BROWSER_POINTER_AUTO;
		content_broadcast(c, CONTENT_MSG_POINTER, msg_data);
	}


	return;
}


/**
 * Handle mouse clicks and movements in a content window.
 *
 * \param  h	  Content handle
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

void content_mouse_action(hlcache_handle *h, struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != NULL);

	if (c->handler->mouse_action != NULL)
		c->handler->mouse_action(c, bw, mouse, x, y);

	return;
}


/**
 * Handle keypresses.
 *
 * \param  h	Content handle
 * \param  key	The UCS4 character codepoint
 * \return true if key handled, false otherwise
 */

bool content_keypress(struct hlcache_handle *h, uint32_t key)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != NULL);

	if (c->handler->keypress != NULL)
		return c->handler->keypress(c, key);

	return false;
}


/**
 * Request a redraw of an area of a content
 *
 * \param h	  high-level cache handle
 * \param x	  x co-ord of left edge
 * \param y	  y co-ord of top edge
 * \param width	  Width of rectangle
 * \param height  Height of rectangle
 */
void content_request_redraw(struct hlcache_handle *h,
		int x, int y, int width, int height)
{
	content__request_redraw(hlcache_handle_get_content(h),
			x, y, width, height);
}


/**
 * Request a redraw of an area of a content
 *
 * \param c	  Content
 * \param x	  x co-ord of left edge
 * \param y	  y co-ord of top edge
 * \param width	  Width of rectangle
 * \param height  Height of rectangle
 */
void content__request_redraw(struct content *c,
		int x, int y, int width, int height)
{
	union content_msg_data data;

	if (c == NULL)
		return;

	data.redraw.x = x;
	data.redraw.y = y;
	data.redraw.width = width;
	data.redraw.height = height;

	data.redraw.full_redraw = true;

	data.redraw.object = c;
	data.redraw.object_x = 0;
	data.redraw.object_y = 0;
	data.redraw.object_width = c->width;
	data.redraw.object_height = c->height;

	content_broadcast(c, CONTENT_MSG_REDRAW, data);
}

/**
 * Display content on screen with optional tiling.
 *
 * Calls the redraw_tile function for the content, or emulates it with the
 * redraw function if it doesn't exist.
 */

bool content_redraw(hlcache_handle *h, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	struct content *c = hlcache_handle_get_content(h);

	assert(c != NULL);

	if (c->locked) {
		/* not safe to attempt redraw */
		return true;
	}

	/* ensure we have a redrawable content */
	if (c->handler->redraw == NULL) {
		return true;
	}

	return c->handler->redraw(c, data, clip, ctx);
}


/**
 * Register a user for callbacks.
 *
 * \param  c	     the content to register
 * \param  callback  the callback function
 * \param  pw	     callback private data
 * \return true on success, false otherwise on memory exhaustion
 *
 * The callback will be called when content_broadcast() is
 * called with the content.
 */

bool content_add_user(struct content *c,
		void (*callback)(struct content *c, content_msg msg,
			union content_msg_data data, void *pw),
		void *pw)
{
	struct content_user *user;

	LOG(("content "URL_FMT_SPC" (%p), user %p %p",
			nsurl_access(llcache_handle_get_url(c->llcache)),
			c, callback, pw));
	user = malloc(sizeof(struct content_user));
	if (!user)
		return false;
	user->callback = callback;
	user->pw = pw;
	user->next = c->user_list->next;
	c->user_list->next = user;

	return true;
}


/**
 * Remove a callback user.
 *
 * The callback function and pw must be identical to those passed to
 * content_add_user().
 */

void content_remove_user(struct content *c,
		void (*callback)(struct content *c, content_msg msg,
			union content_msg_data data, void *pw),
		void *pw)
{
	struct content_user *user, *next;
	LOG(("content "URL_FMT_SPC" (%p), user %p %p",
			nsurl_access(llcache_handle_get_url(c->llcache)), c,
			callback, pw));

	/* user_list starts with a sentinel */
	for (user = c->user_list; user->next != 0 &&
			!(user->next->callback == callback &&
				user->next->pw == pw); user = user->next)
		;
	if (user->next == 0) {
		LOG(("user not found in list"));
		assert(0);
		return;
	}
	next = user->next;
	user->next = next->next;
	free(next);
}

/**
 * Count users for the content.
 */

uint32_t content_count_users(struct content *c)
{
	struct content_user *user;
	uint32_t counter = 0;

	assert(c != NULL);
	
	for (user = c->user_list; user != NULL; user = user->next)
		counter += 1;

	assert(counter > 0);

	return counter - 1; /* Subtract 1 for the sentinel */
}

/**
 * Determine if quirks mode matches
 *
 * \param c       Content to consider
 * \param quirks  Quirks mode to match
 * \return True if quirks match, false otherwise
 */
bool content_matches_quirks(struct content *c, bool quirks)
{
	if (c->handler->matches_quirks == NULL)
		return true;

	return c->handler->matches_quirks(c, quirks);
}

/**
 * Determine if a content is shareable
 *
 * \param c  Content to consider
 * \return True if content is shareable, false otherwise
 */
bool content_is_shareable(struct content *c)
{
	return c->handler->no_share == false;
}

/**
 * Send a message to all users.
 */

void content_broadcast(struct content *c, content_msg msg,
		union content_msg_data data)
{
	struct content_user *user, *next;
	assert(c);
//	LOG(("%p %s -> %d", c, c->url, msg));
	for (user = c->user_list->next; user != 0; user = next) {
		next = user->next;  /* user may be destroyed during callback */
		if (user->callback != 0)
			user->callback(c, msg, data, user->pw);
	}
}

/* exported interface documented in content_protected.h */
void content_broadcast_errorcode(struct content *c, nserror errorcode)
{
	struct content_user *user, *next;
	union content_msg_data data;

	assert(c);

	data.errorcode = errorcode;

	for (user = c->user_list->next; user != 0; user = next) {
		next = user->next;  /* user may be destroyed during callback */
		if (user->callback != 0)
			user->callback(c, CONTENT_MSG_ERRORCODE, data, user->pw);
	}
}


/**
 * A window containing the content has been opened.
 *
 * \param  c	   content that has been opened
 * \param  bw	   browser window containing the content
 * \param  page	   content of type CONTENT_HTML containing c, or 0 if not an
 *		   object within a page
 * \param  params  object parameters, or 0 if not an object
 *
 * Calls the open function for the content.
 */

void content_open(hlcache_handle *h, struct browser_window *bw,
		struct content *page, struct object_params *params)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);
	LOG(("content %p %s", c,
			nsurl_access(llcache_handle_get_url(c->llcache))));
	if (c->handler->open != NULL)
		c->handler->open(c, bw, page, params);
}


/**
 * The window containing the content has been closed.
 *
 * Calls the close function for the content.
 */

void content_close(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);
	LOG(("content %p %s", c,
			nsurl_access(llcache_handle_get_url(c->llcache))));
	if (c->handler->close != NULL)
		c->handler->close(c);
}


/**
 * Tell a content that any selection it has, or one of its objects has, must be
 * cleared.
 */

void content_clear_selection(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);

	if (c->handler->get_selection != NULL)
		c->handler->clear_selection(c);
}


/**
 * Get a text selection from a content.  Ownership is passed to the caller,
 * who must free() it.
 */

char * content_get_selection(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);

	if (c->handler->get_selection != NULL)
		return c->handler->get_selection(c);
	else
		return NULL;
}


void content_get_contextual_content(struct hlcache_handle *h,
		int x, int y, struct contextual_content *data)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);

	if (c->handler->get_contextual_content != NULL) {
		c->handler->get_contextual_content(c, x, y, data);
		return;
	} else {
		data->object = h;
		return;
	}
}


bool content_scroll_at_point(struct hlcache_handle *h,
		int x, int y, int scrx, int scry)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);

	if (c->handler->scroll_at_point != NULL)
		return c->handler->scroll_at_point(c, x, y, scrx, scry);

	return false;
}


bool content_drop_file_at_point(struct hlcache_handle *h,
		int x, int y, char *file)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);

	if (c->handler->drop_file_at_point != NULL)
		return c->handler->drop_file_at_point(c, x, y, file);

	return false;
}


void content_search(struct hlcache_handle *h, void *context,
		search_flags_t flags, const char *string)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);

	if (c->handler->search != NULL) {
		c->handler->search(c, context, flags, string);
	}
}


void content_search_clear(struct hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);

	if (c->handler->search_clear != NULL) {
		c->handler->search_clear(c);
	}
}


void content_debug_dump(struct hlcache_handle *h, FILE *f)
{
	struct content *c = hlcache_handle_get_content(h);
	assert(c != 0);

	if (c->handler->debug_dump != NULL)
		c->handler->debug_dump(c, f);
}


void content_add_error(struct content *c, const char *token,
		unsigned int line)
{
}

bool content__set_title(struct content *c, const char *title)
{
	char *new_title = strdup(title);
	if (new_title == NULL)
		return false;

	if (c->title != NULL)
		free(c->title);

	c->title = new_title;

	return true;
}

struct content_rfc5988_link *
content_find_rfc5988_link(hlcache_handle *h, lwc_string *rel)
{
	struct content *c = hlcache_handle_get_content(h);
	struct content_rfc5988_link *link = c->links;
	bool rel_match = false;

	while (link != NULL) {
		if (lwc_string_caseless_isequal(link->rel, rel,
				&rel_match) == lwc_error_ok && rel_match) {
			break;
		}
		link = link->next;
	}
	return link;
}

struct content_rfc5988_link *
content__free_rfc5988_link(struct content_rfc5988_link *link) 
{
	struct content_rfc5988_link *next;

	next = link->next;

	lwc_string_unref(link->rel);
	nsurl_unref(link->href);
	if (link->hreflang != NULL) {
		lwc_string_unref(link->hreflang);
	}
	if (link->type != NULL) {
		lwc_string_unref(link->type);
	}
	if (link->media != NULL) {
		lwc_string_unref(link->media);
	}
	if (link->sizes != NULL) {
		lwc_string_unref(link->sizes);
	}
	free(link);

	return next;
}

bool content__add_rfc5988_link(struct content *c, 
		const struct content_rfc5988_link *link)
{
	struct content_rfc5988_link *newlink;	
	union content_msg_data msg_data;

	/* a link relation must be present for it to be a link */
	if (link->rel == NULL) {
		return false;
	}

	/* a link href must be present for it to be a link */
	if (link->href == NULL) {
		return false;
	}

	newlink = calloc(1, sizeof(struct content_rfc5988_link));
	if (newlink == NULL) {
		return false; 
	}

	/* copy values */
	newlink->rel = lwc_string_ref(link->rel);
	newlink->href = nsurl_ref(link->href);
	if (link->hreflang != NULL) {
		newlink->hreflang = lwc_string_ref(link->hreflang);
	}
	if (link->type != NULL) {
		newlink->type = lwc_string_ref(link->type);
	}
	if (link->media != NULL) {
		newlink->media = lwc_string_ref(link->media);
	}
	if (link->sizes != NULL) {
		newlink->sizes = lwc_string_ref(link->sizes);
	}

	/* add to metadata link to list */
	newlink->next = c->links;
	c->links = newlink;

	/* broadcast the data */
	msg_data.rfc5988_link = newlink;
	content_broadcast(c, CONTENT_MSG_LINK, msg_data);

	return true;
}

/**
 * Retrieve computed type of content
 *
 * \param c  Content to retrieve type of
 * \return Computed content type
 */
content_type content_get_type(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);

	if (c == NULL)
		return CONTENT_NONE;

	return c->handler->type();
}

/**
 * Retrieve mime-type of content
 *
 * \param c  Content to retrieve mime-type of
 * \return Pointer to referenced mime-type, or NULL if not found.
 */
lwc_string *content_get_mime_type(hlcache_handle *h)
{
	return content__get_mime_type(hlcache_handle_get_content(h));
}

lwc_string *content__get_mime_type(struct content *c)
{
	if (c == NULL)
		return NULL;

	return lwc_string_ref(c->mime_type);
}

/**
 * Retrieve URL associated with content
 *
 * \param c  Content to retrieve URL from
 * \return Pointer to URL, or NULL if not found.
 */
nsurl *content_get_url(struct content *c)
{
	if (c == NULL)
		return NULL;

	return llcache_handle_get_url(c->llcache);
}

/**
 * Retrieve title associated with content
 *
 * \param c  Content to retrieve title from
 * \return Pointer to title, or NULL if not found.
 */
const char *content_get_title(hlcache_handle *h)
{
	return content__get_title(hlcache_handle_get_content(h));
}

const char *content__get_title(struct content *c)
{
	if (c == NULL)
		return NULL;

	return c->title != NULL ? c->title :
			nsurl_access(llcache_handle_get_url(c->llcache));
}

/**
 * Retrieve status of content
 *
 * \param c  Content to retrieve status of
 * \return Content status
 */
content_status content_get_status(hlcache_handle *h)
{
	return content__get_status(hlcache_handle_get_content(h));
}

content_status content__get_status(struct content *c)
{
	if (c == NULL)
		return CONTENT_STATUS_ERROR;

	return c->status;
}

/**
 * Retrieve status message associated with content
 *
 * \param c  Content to retrieve status message from
 * \return Pointer to status message, or NULL if not found.
 */
const char *content_get_status_message(hlcache_handle *h)
{
	return content__get_status_message(hlcache_handle_get_content(h));
}

const char *content__get_status_message(struct content *c)
{
	if (c == NULL)
		return NULL;

	return c->status_message;
}

/**
 * Retrieve width of content
 *
 * \param c  Content to retrieve width of
 * \return Content width
 */
int content_get_width(hlcache_handle *h)
{
	return content__get_width(hlcache_handle_get_content(h));
}

int content__get_width(struct content *c)
{
	if (c == NULL)
		return 0;

	return c->width;
}

/**
 * Retrieve height of content
 *
 * \param c  Content to retrieve height of
 * \return Content height
 */
int content_get_height(hlcache_handle *h)
{
	return content__get_height(hlcache_handle_get_content(h));
}

int content__get_height(struct content *c)
{
	if (c == NULL)
		return 0;

	return c->height;
}

/**
 * Retrieve available width of content
 *
 * \param c  Content to retrieve available width of
 * \return Available width of content
 */
int content_get_available_width(hlcache_handle *h)
{
	return content__get_available_width(hlcache_handle_get_content(h));
}

int content__get_available_width(struct content *c)
{
	if (c == NULL)
		return 0;

	return c->available_width;
}


/**
 * Retrieve source of content
 *
 * \param c	Content to retrieve source of
 * \param size	Pointer to location to receive byte size of source
 * \return Pointer to source data
 */
const char *content_get_source_data(hlcache_handle *h, unsigned long *size)
{
	return content__get_source_data(hlcache_handle_get_content(h), size);
}

const char *content__get_source_data(struct content *c, unsigned long *size)
{
	const uint8_t *data;
	size_t len;

	assert(size != NULL);

	if (c == NULL)
		return NULL;

	data = llcache_handle_get_source_data(c->llcache, &len);

	*size = (unsigned long) len;

	return (const char *) data;
}

/**
 * Invalidate content reuse data: causes subsequent requests for content URL 
 * to query server to determine if content can be reused. This is required 
 * behaviour for forced reloads etc.
 *
 * \param c  Content to invalidate
 */
void content_invalidate_reuse_data(hlcache_handle *h)
{
	content__invalidate_reuse_data(hlcache_handle_get_content(h));
}

void content__invalidate_reuse_data(struct content *c)
{
	if (c == NULL || c->llcache == NULL)
		return;

	/* Invalidate low-level cache data */
	llcache_handle_invalidate_cache_data(c->llcache);
}

/**
 * Retrieve the refresh URL for a content
 *
 * \param c  Content to retrieve refresh URL from
 * \return Pointer to URL, or NULL if none
 */
nsurl *content_get_refresh_url(hlcache_handle *h)
{
	return content__get_refresh_url(hlcache_handle_get_content(h));
}

nsurl *content__get_refresh_url(struct content *c)
{
	if (c == NULL)
		return NULL;

	return c->refresh;
}


/**
 * Retrieve the bitmap contained in an image content
 *
 * \param c  Content to retrieve bitmap from
 * \return Pointer to bitmap, or NULL if none.
 */
struct bitmap *content_get_bitmap(hlcache_handle *h)
{
	return content__get_bitmap(hlcache_handle_get_content(h));
}

struct bitmap *content__get_bitmap(struct content *c)
{
	struct bitmap *bitmap = NULL;

	if ((c != NULL) && 
	    (c->handler != NULL) && 
	    (c->handler->type != NULL) && 
	    (c->handler->type() == CONTENT_IMAGE) &&
	    (c->handler->get_internal != NULL) ) {
		bitmap = c->handler->get_internal(c, NULL);
	}

	return bitmap;
}


/**
 * Determine if a content is opaque from handle
 *
 * \param h high level cache handle to retrieve opacity from.
 * \return false if the content is not opaque or information is not
 *         known else true.
 */
bool content_get_opaque(hlcache_handle *h)
{
	return content__get_opaque(hlcache_handle_get_content(h));
}

/**
 * Determine if a content is opaque
 *
 * \param c Content to retrieve opacity from
 * \return false if the content is not opaque or information is not
 *         known else true.
 */
bool content__get_opaque(struct content *c)
{
	bool opaque = false;

	if ((c != NULL) && 
	    (c->handler != NULL) && 
	    (c->handler->type != NULL) && 
	    (c->handler->type() == CONTENT_IMAGE) &&
	    (c->handler->get_internal != NULL) ) {
		struct bitmap *bitmap = NULL;
		bitmap = c->handler->get_internal(c, NULL);
		if (bitmap != NULL) { 
			opaque = bitmap_get_opaque(bitmap);
		}
	}

	return opaque;
}


/**
 * Retrieve quirkiness of a content
 *
 * \param h  Content to examine
 * \return True if content is quirky, false otherwise
 */
bool content_get_quirks(hlcache_handle *h)
{
	struct content *c = hlcache_handle_get_content(h);

	if (c == NULL)
		return false;

	return c->quirks;
}


/**
 * Return whether a content is currently locked
 *
 * \param c  Content to test
 * \return true iff locked, else false
 */

bool content_is_locked(hlcache_handle *h)
{
	return content__is_locked(hlcache_handle_get_content(h));
}

bool content__is_locked(struct content *c)
{
	return c->locked;
}

/**
 * Retrieve the low-level cache handle for a content
 *
 * \param h  Content to retrieve from
 * \return Low-level cache handle
 */
const llcache_handle *content_get_llcache_handle(struct content *c)
{
	if (c == NULL)
		return NULL;

	return c->llcache;
}

/**
 * Clone a content object in its current state.
 *
 * \param c  Content to clone
 * \return Clone of \a c
 */
struct content *content_clone(struct content *c)
{
	struct content *nc;
	nserror error;

	error = c->handler->clone(c, &nc);
	if (error != NSERROR_OK)
		return NULL;

	return nc;
};

/**
 * Clone a content's data members
 *
 * \param c   Content to clone
 * \param nc  Content to populate
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror content__clone(const struct content *c, struct content *nc)
{
	nserror error;

	error = llcache_handle_clone(c->llcache, &(nc->llcache));
	if (error != NSERROR_OK) {
		return error;
	}

	llcache_handle_change_callback(nc->llcache,
				       content_llcache_callback, nc);

	nc->mime_type = lwc_string_ref(c->mime_type);
	nc->handler = c->handler;

	nc->status = c->status;

	nc->width = c->width;
	nc->height = c->height;
	nc->available_width = c->available_width;
	nc->quirks = c->quirks;

	if (c->fallback_charset != NULL) {
		nc->fallback_charset = strdup(c->fallback_charset);
		if (nc->fallback_charset == NULL) {
			return NSERROR_NOMEM;
		}
	}

	if (c->refresh != NULL) {
		nc->refresh = nsurl_ref(c->refresh);
		if (nc->refresh == NULL) {
			return NSERROR_NOMEM;
		}
	}

	nc->time = c->time;
	nc->reformat_time = c->reformat_time;
	nc->size = c->size;

	if (c->title != NULL) {
		nc->title = strdup(c->title);
		if (nc->title == NULL) {
			return NSERROR_NOMEM;
		}
	}

	nc->active = c->active;

	nc->user_list = calloc(1, sizeof(struct content_user));
	if (nc->user_list == NULL) {
		return NSERROR_NOMEM;
	}

	memcpy(&(nc->status_message), &(c->status_message), 120);
	memcpy(&(nc->sub_status), &(c->sub_status), 80);

	nc->locked = c->locked;
	nc->total_size = c->total_size;
	nc->http_code = c->http_code;
	
	return NSERROR_OK;
}

/**
 * Abort a content object
 *
 * \param c The content object to abort
 * \return NSERROR_OK on success, otherwise appropriate error
 */
nserror content_abort(struct content *c)
{
	LOG(("Aborting %p", c));
	
	if (c->handler->stop != NULL)
		c->handler->stop(c);
	
	/* And for now, abort our llcache object */
	return llcache_handle_abort(c->llcache);
}

