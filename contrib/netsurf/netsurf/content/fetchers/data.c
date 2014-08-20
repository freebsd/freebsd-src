/*
 * Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
 *
 * This file is part of NetSurf.
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

/* data: URL handling.  See http://tools.ietf.org/html/rfc2397 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <curl/curl.h>		/* for URL unescaping functions */

#include <libwapcaplet/libwapcaplet.h>

#include "utils/config.h"
#include "content/fetch.h"
#include "content/fetchers/data.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "utils/corestrings.h"
#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/ring.h"
#include "utils/base64.h"

struct fetch_data_context {
	struct fetch *parent_fetch;
	char *url;
	char *mimetype;
	char *data;
	size_t datalen;
	bool base64;

	bool aborted;
	bool locked;
	
	struct fetch_data_context *r_next, *r_prev;
};

static struct fetch_data_context *ring = NULL;

static CURL *curl;

static bool fetch_data_initialise(lwc_string *scheme)
{
	LOG(("fetch_data_initialise called for %s", lwc_string_data(scheme)));
	if ( (curl = curl_easy_init()) == NULL)
		return false;
	else
		return true;
}

static void fetch_data_finalise(lwc_string *scheme)
{
	LOG(("fetch_data_finalise called for %s", lwc_string_data(scheme)));
	curl_easy_cleanup(curl);
}

static bool fetch_data_can_fetch(const nsurl *url)
{
	return true;
}

static void *fetch_data_setup(struct fetch *parent_fetch, nsurl *url,
		 bool only_2xx, bool downgrade_tls, const char *post_urlenc,
		 const struct fetch_multipart_data *post_multipart,
		 const char **headers)
{
	struct fetch_data_context *ctx = calloc(1, sizeof(*ctx));
	
	if (ctx == NULL)
		return NULL;
		
	ctx->parent_fetch = parent_fetch;

	/* TODO: keep as nsurl to avoid copy */
	ctx->url = malloc(nsurl_length(url) + 1);
	if (ctx->url == NULL) {
		free(ctx);
		return NULL;
	}
	memcpy(ctx->url, nsurl_access(url), nsurl_length(url) + 1);

	RING_INSERT(ring, ctx);
	
	return ctx;
}

static bool fetch_data_start(void *ctx)
{
	return true;
}

static void fetch_data_free(void *ctx)
{
	struct fetch_data_context *c = ctx;

	free(c->url);
	free(c->data);
	free(c->mimetype);
	RING_REMOVE(ring, c);
	free(ctx);
}

static void fetch_data_abort(void *ctx)
{
	struct fetch_data_context *c = ctx;

	/* To avoid the poll loop having to deal with the fetch context
	 * disappearing from under it, we simply flag the abort here. 
	 * The poll loop itself will perform the appropriate cleanup.
	 */
	c->aborted = true;
}

static void fetch_data_send_callback(const fetch_msg *msg, 
		struct fetch_data_context *c) 
{
	c->locked = true;
	fetch_send_callback(msg, c->parent_fetch);
	c->locked = false;
}

static bool fetch_data_process(struct fetch_data_context *c)
{
	fetch_msg msg;
	char *params;
	char *comma;
	char *unescaped;
        int templen;
	
	/* format of a data: URL is:
	 *   data:[<mimetype>][;base64],<data>
	 * The mimetype is optional.  If it is missing, the , before the
	 * data must still be there.
	 */
	
	LOG(("url: %.140s", c->url));
	
	if (strlen(c->url) < 6) {
		/* 6 is the minimum possible length (data:,) */
		msg.type = FETCH_ERROR;
		msg.data.error = "Malformed data: URL";
		fetch_data_send_callback(&msg, c);
		return false;
	}
	
	/* skip the data: part */
	params = c->url + SLEN("data:");
	
	/* find the comma */
	if ( (comma = strchr(params, ',')) == NULL) {
		msg.type = FETCH_ERROR;
		msg.data.error = "Malformed data: URL";
		fetch_data_send_callback(&msg, c);
		return false;
	}
	
	if (params[0] == ',') {
		/* there is no mimetype here, assume text/plain */
		c->mimetype = strdup("text/plain;charset=US-ASCII");
	} else {	
		/* make a copy of everything between data: and the comma */
		c->mimetype = strndup(params, comma - params);
	}
	
	if (c->mimetype == NULL) {
		msg.type = FETCH_ERROR;
		msg.data.error = 
			"Unable to allocate memory for mimetype in data: URL";
		fetch_data_send_callback(&msg, c);
		return false;
	}
	
	if (strcmp(c->mimetype + strlen(c->mimetype) - 7, ";base64") == 0) {
		c->base64 = true;
		c->mimetype[strlen(c->mimetype) - 7] = '\0';
	} else {
		c->base64 = false;
	}
	
	/* we URL unescape the data first, just incase some insane page
	 * decides to nest URL and base64 encoding.  Like, say, Acid2.
	 */
        templen = c->datalen;
        unescaped = curl_easy_unescape(curl, comma + 1, 0, &templen);
        c->datalen = templen;
        if (unescaped == NULL) {
		msg.type = FETCH_ERROR;
		msg.data.error = "Unable to URL decode data: URL";
		fetch_data_send_callback(&msg, c);
		return false;
	}
	
	if (c->base64) {
		c->data = malloc(c->datalen); /* safe: always gets smaller */
		if (base64_decode(unescaped, c->datalen, c->data,
				&(c->datalen)) == false) {
			msg.type = FETCH_ERROR;
			msg.data.error = "Unable to Base64 decode data: URL";
			fetch_data_send_callback(&msg, c);
			curl_free(unescaped);
			return false;
		}
	} else {
		c->data = malloc(c->datalen);
		if (c->data == NULL) {
			msg.type = FETCH_ERROR;
			msg.data.error =
				"Unable to allocate memory for data: URL";
			fetch_data_send_callback(&msg, c);
			curl_free(unescaped);
			return false;
		}
		memcpy(c->data, unescaped, c->datalen);
	}
	
	curl_free(unescaped);
	
	return true;
}

static void fetch_data_poll(lwc_string *scheme)
{
	fetch_msg msg;
	struct fetch_data_context *c, *next;
	
	if (ring == NULL) return;
	
	/* Iterate over ring, processing each pending fetch */
	c = ring;
	do {
		/* Ignore fetches that have been flagged as locked.
		 * This allows safe re-entrant calls to this function.
		 * Re-entrancy can occur if, as a result of a callback,
		 * the interested party causes fetch_poll() to be called 
		 * again.
		 */
		if (c->locked == true) {
			next = c->r_next;
			continue;
		}

		/* Only process non-aborted fetches */
		if (c->aborted == false && fetch_data_process(c) == true) {
			char header[64];

			fetch_set_http_code(c->parent_fetch, 200);
			LOG(("setting data: MIME type to %s, length to %zd",
					c->mimetype, c->datalen));
			/* Any callback can result in the fetch being aborted.
			 * Therefore, we _must_ check for this after _every_
			 * call to fetch_data_send_callback().
			 */
			snprintf(header, sizeof header, "Content-Type: %s",
					c->mimetype);
			msg.type = FETCH_HEADER;
			msg.data.header_or_data.buf = (const uint8_t *) header;
			msg.data.header_or_data.len = strlen(header);
			fetch_data_send_callback(&msg, c); 

			if (c->aborted == false) {
				snprintf(header, sizeof header, 
					"Content-Length: %"SSIZET_FMT,
					c->datalen);
				msg.type = FETCH_HEADER;
				msg.data.header_or_data.buf = 
						(const uint8_t *) header;
				msg.data.header_or_data.len = strlen(header);
				fetch_data_send_callback(&msg, c);
			}

			if (c->aborted == false) {
				msg.type = FETCH_DATA;
				msg.data.header_or_data.buf = 
						(const uint8_t *) c->data;
				msg.data.header_or_data.len = c->datalen;
				fetch_data_send_callback(&msg, c);
			}

			if (c->aborted == false) {
				msg.type = FETCH_FINISHED;
				fetch_data_send_callback(&msg, c);
			}
		} else {
			LOG(("Processing of %s failed!", c->url));

			/* Ensure that we're unlocked here. If we aren't, 
			 * then fetch_data_process() is broken.
			 */
			assert(c->locked == false);
		}

		/* Compute next fetch item at the last possible moment as
		 * processing this item may have added to the ring.
		 */
		next = c->r_next;

		fetch_remove_from_queues(c->parent_fetch);
		fetch_free(c->parent_fetch);

		/* Advance to next ring entry, exiting if we've reached
		 * the start of the ring or the ring has become empty
		 */
	} while ( (c = next) != ring && ring != NULL);
}

void fetch_data_register(void)
{
	lwc_string *scheme = lwc_string_ref(corestring_lwc_data);

	fetch_add_fetcher(scheme,
		fetch_data_initialise,
		fetch_data_can_fetch,
		fetch_data_setup,
		fetch_data_start,
		fetch_data_abort,
		fetch_data_free,
		fetch_data_poll,
		fetch_data_finalise);
}
