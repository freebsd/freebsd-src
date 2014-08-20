/*
 * Copyright 2005-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Philip Pemberton <philpem@users.sourceforge.net>
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
 * Content handling (interface).
 *
 * The content functions manipulate struct contents, which correspond to URLs.
 */

#ifndef _NETSURF_CONTENT_CONTENT_PROTECTED_H_
#define _NETSURF_CONTENT_CONTENT_PROTECTED_H_

#include <stdint.h>
#include <time.h>
#include "utils/config.h"
#include "content/content.h"
#include "content/content_factory.h"
#include "content/llcache.h"
#include "utils/errors.h"

struct bitmap;
struct content;
struct rect;
struct redraw_context;

struct content_handler {
	void (*fini)(void);

	nserror (*create)(const content_handler *handler,
			lwc_string *imime_type, const http_parameter *params,
			llcache_handle *llcache, 
			const char *fallback_charset, bool quirks, 
			struct content **c);

	bool (*process_data)(struct content *c, 
			const char *data, unsigned int size);
	bool (*data_complete)(struct content *c);
	void (*reformat)(struct content *c, int width, int height);
	void (*destroy)(struct content *c);
	void (*stop)(struct content *c);
	void (*mouse_track)(struct content *c, struct browser_window *bw,
			browser_mouse_state mouse, int x, int y);
	void (*mouse_action)(struct content *c, struct browser_window *bw,
			browser_mouse_state mouse, int x, int y);
	bool (*keypress)(struct content *c, uint32_t key);
	bool (*redraw)(struct content *c, struct content_redraw_data *data,
			const struct rect *clip,
			const struct redraw_context *ctx);
	void (*open)(struct content *c, struct browser_window *bw,
			struct content *page, struct object_params *params);
	void (*close)(struct content *c);
	void (*clear_selection)(struct content *c);
	char * (*get_selection)(struct content *c);
	void (*get_contextual_content)(struct content *c, int x, int y,
			struct contextual_content *data);
	bool (*scroll_at_point)(struct content *c, int x, int y,
			int scrx, int scry);
	bool (*drop_file_at_point)(struct content *c, int x, int y,
			char *file);
	void (*search)(struct content *c, void *context, search_flags_t flags,
		       const char *string);
	void (*search_clear)(struct content *c);
	void (*debug_dump)(struct content *c, FILE *f);
	nserror (*clone)(const struct content *old, struct content **newc);
	bool (*matches_quirks)(const struct content *c, bool quirks);
	content_type (*type)(void);

        /** handler dependant content sensitive internal data interface. */
	void * (*get_internal)(const struct content *c, void *context);

	/** There must be one content per user for this type. */
	bool no_share;
};

/** Linked list of users of a content. */
struct content_user
{
	void (*callback)(struct content *c, content_msg msg,
			union content_msg_data data, void *pw);
	void *pw;

	struct content_user *next;
};

/** Corresponds to a single URL. */
struct content {
	llcache_handle *llcache;	/**< Low-level cache object */

	lwc_string *mime_type;	/**< Original MIME type of data */

	const content_handler *handler;	/**< Handler for content */

	content_status status;	/**< Current status. */

	int width, height;	/**< Dimensions, if applicable. */
	int available_width;	/**< Available width (eg window width). */

	bool quirks;		/**< Content is in quirks mode */
	char *fallback_charset;	/**< Fallback charset, or NULL */

	nsurl *refresh;		/**< URL for refresh request */

	struct content_rfc5988_link *links; /**< list of metadata links */

	unsigned int time;		/**< Creation time,
					  if LOADING or READY,
					  otherwise total time. */

	unsigned int reformat_time;	/**< Earliest time to attempt a
					  period reflow while fetching a
					  page's objects. */

	unsigned int size;		/**< Estimated size of all data
					  associated with this content */
	char *title;			/**< Title for browser window. */
	unsigned int active;		/**< Number of child fetches or
					  conversions currently in progress. */
	struct content_user *user_list;	/**< List of users. */
	char status_message[120];	/**< Full text for status bar. */
	char sub_status[80];		/**< Status of content. */
	/** Content is being processed: data structures may be inconsistent
	 * and content must not be redrawn or modified. */
	bool locked;

	unsigned long total_size;	/**< Total data size, 0 if unknown. */
	long http_code;			/**< HTTP status code, 0 if not HTTP. */

	/** Array of first n rendering errors or warnings. */
	struct {
		const char *token;
		unsigned int line;	/**< Line no, 0 if not applicable. */
	} error_list[40];
	unsigned int error_count;	/**< Number of valid error entries. */
};

extern const char * const content_type_name[];
extern const char * const content_status_name[];

nserror content__init(struct content *c, const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		struct llcache_handle *llcache, const char *fallback_charset,
		bool quirks);
nserror content__clone(const struct content *c, struct content *nc);

void content_set_ready(struct content *c);
void content_set_done(struct content *c);
void content_set_error(struct content *c);

void content_set_status(struct content *c, const char *status_message);
void content_broadcast(struct content *c, content_msg msg,
		union content_msg_data data);
/**
 * Send an errorcode message to all users.
 */
void content_broadcast_errorcode(struct content *c, nserror errorcode);

void content_add_error(struct content *c, const char *token,
		unsigned int line);

bool content__add_rfc5988_link(struct content *c, 
		const struct content_rfc5988_link *link);
struct content_rfc5988_link *content__free_rfc5988_link(
		struct content_rfc5988_link *link);

void content__reformat(struct content *c, bool background,
		int width, int height);
void content__request_redraw(struct content *c,
		int x, int y, int width, int height);

bool content__set_title(struct content *c, const char *title);

lwc_string *content__get_mime_type(struct content *c);
const char *content__get_title(struct content *c);
const char *content__get_status_message(struct content *c);
int content__get_width(struct content *c);
int content__get_height(struct content *c);
int content__get_available_width(struct content *c);
const char *content__get_source_data(struct content *c, unsigned long *size);
void content__invalidate_reuse_data(struct content *c);
nsurl *content__get_refresh_url(struct content *c);
struct bitmap *content__get_bitmap(struct content *c);
bool content__get_opaque(struct content *c);

bool content__is_locked(struct content *c);

#endif
