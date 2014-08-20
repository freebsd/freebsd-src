/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

/* rsrc: URL handling. */

#define __STDBOOL_H__	1
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <curl/curl.h>		/* for URL unescaping functions */
extern "C" {
#include "utils/config.h"
#include "content/fetch.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/ring.h"
#include "utils/base64.h"
}
#include "beos/fetch_rsrc.h"
#include "beos/filetype.h"
#include "beos/gui.h"

#include <image.h>
#include <Resources.h>
#include <String.h>

struct fetch_rsrc_context {
	struct fetch *parent_fetch;
	char *name;
	char *url;
	char *mimetype;
	char *data;
	size_t datalen;

	bool aborted;
	bool locked;
	
	struct fetch_rsrc_context *r_next, *r_prev;
};

static struct fetch_rsrc_context *ring = NULL;

BResources *gAppResources = NULL;

static bool fetch_rsrc_initialise(lwc_string *scheme)
{
	LOG(("fetch_rsrc_initialise called for %s", lwc_string_data(scheme)));
	return true;
}

static void fetch_rsrc_finalise(lwc_string *scheme)
{
	LOG(("fetch_rsrc_finalise called for %s", lwc_string_data(scheme)));
}

static bool fetch_rsrc_can_fetch(const nsurl *url)
{
	return true;
}

static void *fetch_rsrc_setup(struct fetch *parent_fetch, nsurl *url,
		 bool only_2xx, bool downgrade_tls, const char *post_urlenc,
		 const struct fetch_multipart_data *post_multipart,
		 const char **headers)
{
	struct fetch_rsrc_context *ctx;
	ctx = (struct fetch_rsrc_context *)calloc(1, sizeof(*ctx));
	
	if (ctx == NULL)
		return NULL;
		
	ctx->parent_fetch = parent_fetch;
	/* TODO: keep as nsurl to avoid copy */
	ctx->url = (char *)malloc(nsurl_length(url) + 1);
	
	if (ctx->url == NULL) {
		free(ctx);
		return NULL;
	}
	memcpy(ctx->url, nsurl_access(url), nsurl_length(url) + 1);

	RING_INSERT(ring, ctx);
	
	return ctx;
}

static bool fetch_rsrc_start(void *ctx)
{
	return true;
}

static void fetch_rsrc_free(void *ctx)
{
	struct fetch_rsrc_context *c = (struct fetch_rsrc_context *)ctx;

	free(c->name);
	free(c->url);
	free(c->data);
	free(c->mimetype);
	RING_REMOVE(ring, c);
	free(ctx);
}

static void fetch_rsrc_abort(void *ctx)
{
	struct fetch_rsrc_context *c = (struct fetch_rsrc_context *)ctx;

	/* To avoid the poll loop having to deal with the fetch context
	 * disappearing from under it, we simply flag the abort here. 
	 * The poll loop itself will perform the appropriate cleanup.
	 */
	c->aborted = true;
}

static void fetch_rsrc_send_callback(const fetch_msg *msg, 
		struct fetch_rsrc_context *c)
{
	c->locked = true;
	fetch_send_callback(msg, c->parent_fetch);
	c->locked = false;
}

static bool fetch_rsrc_process(struct fetch_rsrc_context *c)
{
	fetch_msg msg;
	char *params;
	char *at = NULL;
	char *slash;
	char *comma = NULL;
	char *unescaped;
	uint32 type = 'data'; // default for embeded files
	int32 id = 0;
	
	/* format of a rsrc: URL is:
	 *   rsrc://[TYPE][@NUM]/name[,mime]
	 */
	
	LOG(("*** Processing %s", c->url));
	
	if (strlen(c->url) < 7) {
		/* 7 is the minimum possible length (rsrc://) */
		msg.type = FETCH_ERROR;
		msg.data.error = "Malformed rsrc: URL";
		fetch_rsrc_send_callback(&msg, c);
		return false;
	}
	
	/* skip the rsrc: part */
	params = c->url + sizeof("rsrc://") - 1;
	
	/* find the slash */
	if ( (slash = strchr(params, '/')) == NULL) {
		msg.type = FETCH_ERROR;
		msg.data.error = "Malformed rsrc: URL";
		fetch_rsrc_send_callback(&msg, c);
		return false;
	}

	// doesn't exist in the filesystem but we should hit the internal types.
	c->mimetype = strdup(fetch_filetype(slash));
	c->name = strdup(slash + 1);
	
	if (c->mimetype == NULL) {
		msg.type = FETCH_ERROR;
		msg.data.error =
			"Unable to allocate memory for mimetype in rsrc: URL";
		fetch_rsrc_send_callback(&msg, c);
		return false;
	}

	if (params[0] != '/') {
		uint8 c1, c2, c3, c4;
		if (sscanf(params, "%c%c%c%c", &c1, &c2, &c3, &c4) > 3) {
			type = c1 << 24 | c2 << 16 | c3 << 8 | c4;
			LOG(("fetch_rsrc: type:%4.4s\n", &type));
		}
	}

	LOG(("fetch_rsrc: 0x%08lx, %ld, '%s'\n", type, id, c->name));

	bool found;
	if (id)
		found = gAppResources->HasResource(type, id);
	else
		found = gAppResources->HasResource(type, c->name);
	if (!found) {
		BString error("Cannot locate resource: ");
		if (id)
			error << id;
		else
			error << c->name;
		msg.type = FETCH_ERROR;
		msg.data.error = error.String();
		fetch_rsrc_send_callback(&msg, c);
		return false;
	}

	size_t len;
	const void *data;
	if (id)
		data = gAppResources->LoadResource(type, id, &len);
	else
		data = gAppResources->LoadResource(type, c->name, &len);

	if (!data) {
		msg.type = FETCH_ERROR;
		msg.data.error = "Cannot load rsrc: URL";
		fetch_rsrc_send_callback(&msg, c);
		return false;
	}

	c->datalen = len;
	c->data = (char *)malloc(c->datalen);
	if (c->data == NULL) {
		msg.type = FETCH_ERROR;
		msg.data.error = "Unable to allocate memory for rsrc: URL";
		fetch_rsrc_send_callback(&msg, c);
		return false;
	}
	memcpy(c->data, data, c->datalen);

	return true;
}

static void fetch_rsrc_poll(lwc_string *scheme)
{
	fetch_msg msg;
	struct fetch_rsrc_context *c, *next;

	if (ring == NULL) return;
	
	/* Iterate over ring, processing each pending fetch */
	c = ring;
	do {
		/* Take a copy of the next pointer as we may destroy
		 * the ring item we're currently processing */
		next = c->r_next;

		/* Ignore fetches that have been flagged as locked.
		 * This allows safe re-entrant calls to this function.
		 * Re-entrancy can occur if, as a result of a callback,
		 * the interested party causes fetch_poll() to be called 
		 * again.
		 */
		if (c->locked == true) {
			continue;
		}

		/* Only process non-aborted fetches */
		if (!c->aborted && fetch_rsrc_process(c) == true) {
			char header[64];

			fetch_set_http_code(c->parent_fetch, 200);
			LOG(("setting rsrc: MIME type to %s, length to %zd",
					c->mimetype, c->datalen));
			/* Any callback can result in the fetch being aborted.
			 * Therefore, we _must_ check for this after _every_
			 * call to fetch_rsrc_send_callback().
			 */
			snprintf(header, sizeof header, "Content-Type: %s",
					c->mimetype);
			msg.type = FETCH_HEADER;
			msg.data.header_or_data.buf = (const uint8_t *) header;
			msg.data.header_or_data.len = strlen(header);
			fetch_rsrc_send_callback(&msg, c);

			snprintf(header, sizeof header, "Content-Length: %zd",
					c->datalen);
			msg.type = FETCH_HEADER;
			msg.data.header_or_data.buf = (const uint8_t *) header;
			msg.data.header_or_data.len = strlen(header);
			fetch_rsrc_send_callback(&msg, c);

			if (!c->aborted) {
				msg.type = FETCH_DATA;
				msg.data.header_or_data.buf = (const uint8_t *) c->data;
				msg.data.header_or_data.len = c->datalen;
				fetch_rsrc_send_callback(&msg, c);
			}
			if (!c->aborted) {
				msg.type = FETCH_FINISHED;
				fetch_rsrc_send_callback(&msg, c);
			}
		} else {
			LOG(("Processing of %s failed!", c->url));

			/* Ensure that we're unlocked here. If we aren't, 
			 * then fetch_rsrc_process() is broken.
			 */
			assert(c->locked == false);
		}

		fetch_remove_from_queues(c->parent_fetch);
		fetch_free(c->parent_fetch);

		/* Advance to next ring entry, exiting if we've reached
		 * the start of the ring or the ring has become empty
		 */
	} while ( (c = next) != ring && ring != NULL);
}

/* BAppFileInfo is supposed to find the app's resources for us,
 * but this won't work if we ever want to be used as a replicant.
 * This trick should work regardless,
 */
static int find_app_resources()
{
	char path[B_PATH_NAME_LENGTH];
	if (nsbeos_find_app_path(path) < B_OK)
		return B_ERROR;
	//fprintf(stderr, "loading resources from '%s'\n", path);

	BFile file(path, B_READ_ONLY);
	if (file.InitCheck() < 0)
		return file.InitCheck();
	gAppResources = new BResources;
	status_t err;
	err = gAppResources->SetTo(&file);
	if (err >= B_OK)
		return B_OK;
	delete gAppResources;
	gAppResources = NULL;
	return err;
}

BResources *get_app_resources()
{
	return gAppResources;
}

void fetch_rsrc_register(void)
{
	lwc_string *scheme;
	int err;

	err = find_app_resources();

	if (err < B_OK) {
		warn_user("Resources", strerror(err));
		return;
	}

	if (lwc_intern_string("rsrc", SLEN("rsrc"), &scheme) != lwc_error_ok) {
		die("Failed to initialise the fetch module "
				"(couldn't intern \"rsrc\").");
	}

	fetch_add_fetcher(scheme,
		fetch_rsrc_initialise,
		fetch_rsrc_can_fetch,
		fetch_rsrc_setup,
		fetch_rsrc_start,
		fetch_rsrc_abort,
		fetch_rsrc_free,
		fetch_rsrc_poll,
		fetch_rsrc_finalise);
}

void fetch_rsrc_unregister(void)
{
	delete gAppResources;
	gAppResources = NULL;
}
