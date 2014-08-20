/*
 * Copyright 2010 John-Mark Bell <jmb@netsurf-browser.org>
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

/**
 * \file desktop/download.c
 * \brief Core download context implementation
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "content/llcache.h"
#include "desktop/download.h"
#include "desktop/gui_factory.h"
#include "utils/corestrings.h"
#include "utils/http.h"
#include "utils/url.h"
#include "utils/utils.h"

/**
 * A context for a download
 */
struct download_context {
	llcache_handle *llcache;		/**< Low-level cache handle */
	struct gui_window *parent;		/**< Parent window */

	lwc_string *mime_type;			/**< MIME type of download */
	unsigned long total_length;		/**< Length of data, in bytes */
	char *filename;				/**< Suggested filename */

	struct gui_download_window *window;	/**< GUI download window */
};

/**
 * Parse a filename parameter value
 * 
 * \param filename  Value to parse
 * \return Sanitised filename, or NULL on memory exhaustion
 */
static char *download_parse_filename(const char *filename)
{
	const char *slash = strrchr(filename, '/');

	if (slash != NULL)
		slash++;
	else
		slash = filename;

	return strdup(slash);
}

/**
 * Compute a default filename for a download
 *
 * \param url  URL of item being fetched
 * \return Default filename, or NULL on memory exhaustion
 */
static char *download_default_filename(const char *url)
{
	char *nice;

	if (url_nice(url, &nice, false) == URL_FUNC_OK)
		return nice;

	return NULL;
}

/**
 * Process fetch headers for a download context.
 * Extracts MIME type, total length, and creates gui_download_window
 *
 * \param ctx  Context to process
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror download_context_process_headers(download_context *ctx)
{
	const char *http_header;
	http_content_type *content_type;
	unsigned long length;
	nserror error;

	/* Retrieve and parse Content-Type */
	http_header = llcache_handle_get_header(ctx->llcache, "Content-Type");
	if (http_header == NULL)
		http_header = "text/plain";

	error = http_parse_content_type(http_header, &content_type);
	if (error != NSERROR_OK)
		return error;

	/* Retrieve and parse Content-Length */
	http_header = llcache_handle_get_header(ctx->llcache, "Content-Length");
	if (http_header == NULL)
		length = 0;
	else
		length = strtoul(http_header, NULL, 10);

	/* Retrieve and parse Content-Disposition */
	http_header = llcache_handle_get_header(ctx->llcache, 
			"Content-Disposition");
	if (http_header != NULL) {
		lwc_string *filename_value;
		http_content_disposition *disposition;

		error = http_parse_content_disposition(http_header, 
				&disposition);
		if (error != NSERROR_OK) {
			http_content_type_destroy(content_type);
			return error;
		}

		error = http_parameter_list_find_item(disposition->parameters, 
				corestring_lwc_filename, &filename_value);
		if (error == NSERROR_OK) {
			ctx->filename = download_parse_filename(
					lwc_string_data(filename_value));
			lwc_string_unref(filename_value);
		}

		http_content_disposition_destroy(disposition);
	}

	ctx->mime_type = lwc_string_ref(content_type->media_type);
	ctx->total_length = length;
	if (ctx->filename == NULL) {
		ctx->filename = download_default_filename(
				nsurl_access(
				llcache_handle_get_url(ctx->llcache)));
	}

	http_content_type_destroy(content_type);

	if (ctx->filename == NULL) {
		lwc_string_unref(ctx->mime_type);
		ctx->mime_type = NULL;
		return NSERROR_NOMEM;
	}

	/* Create the frontend window */
	ctx->window = guit->download->create(ctx, ctx->parent);
	if (ctx->window == NULL) {
		free(ctx->filename);
		ctx->filename = NULL;
		lwc_string_unref(ctx->mime_type);
		ctx->mime_type = NULL;
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}

/**
 * Callback for low-level cache events
 *
 * \param handle  Low-level cache handle
 * \param event   Event object
 * \param pw      Our context
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror download_callback(llcache_handle *handle,
		const llcache_event *event, void *pw)
{
	download_context *ctx = pw;
	nserror error = NSERROR_OK;

	switch (event->type) {
	case LLCACHE_EVENT_HAD_HEADERS:
		error = download_context_process_headers(ctx);
		if (error != NSERROR_OK) {
			llcache_handle_abort(handle);
			download_context_destroy(ctx);
		}

		break;

	case LLCACHE_EVENT_HAD_DATA:
		/* If we didn't know up-front that this fetch was for download,
		 * then we won't receive the HAD_HEADERS event. Catch up now.
		 */
		if (ctx->window == NULL) {
			error = download_context_process_headers(ctx);
			if (error != NSERROR_OK) {
				llcache_handle_abort(handle);
				download_context_destroy(ctx);
			}
		}

		if (error == NSERROR_OK) {
			/** \todo Lose ugly cast */
			error = guit->download->data(ctx->window,
					(char *) event->data.data.buf,
					event->data.data.len);
			if (error != NSERROR_OK)
				llcache_handle_abort(handle);
		}

		break;

	case LLCACHE_EVENT_DONE:
		/* There may be no associated window if there was no data or headers */
		if (ctx->window != NULL)
			guit->download->done(ctx->window);
		else
			download_context_destroy(ctx);

		break;

	case LLCACHE_EVENT_ERROR:
		if (ctx->window != NULL)
			guit->download->error(ctx->window, event->data.msg);
		else
			download_context_destroy(ctx);

		break;

	case LLCACHE_EVENT_PROGRESS:
		break;

	case LLCACHE_EVENT_REDIRECT:
		break;
	}

	return error;
}

/* See download.h for documentation */
nserror download_context_create(llcache_handle *llcache, 
		struct gui_window *parent)
{
	download_context *ctx;

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL)
		return NSERROR_NOMEM;

	ctx->llcache = llcache;
	ctx->parent = parent;
	ctx->mime_type = NULL;
	ctx->total_length = 0;
	ctx->filename = NULL;
	ctx->window = NULL;

	llcache_handle_change_callback(llcache, download_callback, ctx);

	return NSERROR_OK;
}

/* See download.h for documentation */
void download_context_destroy(download_context *ctx)
{
	llcache_handle_release(ctx->llcache);

	if (ctx->mime_type != NULL)
		lwc_string_unref(ctx->mime_type);

	free(ctx->filename);

	/* Window is not owned by us, so don't attempt to destroy it */

	free(ctx);
}

/* See download.h for documentation */
void download_context_abort(download_context *ctx)
{
	llcache_handle_abort(ctx->llcache);
}

/* See download.h for documentation */
const char *download_context_get_url(const download_context *ctx)
{
	return nsurl_access(llcache_handle_get_url(ctx->llcache));
}

/* See download.h for documentation */
const char *download_context_get_mime_type(const download_context *ctx)
{
	return lwc_string_data(ctx->mime_type);
}

/* See download.h for documentation */
unsigned long download_context_get_total_length(const download_context *ctx)
{
	return ctx->total_length;
}

/* See download.h for documentation */
const char *download_context_get_filename(const download_context *ctx)
{
	return ctx->filename;
}

