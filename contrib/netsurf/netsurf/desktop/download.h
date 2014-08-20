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
 * \file desktop/download.h
 * \brief Core download context (interface)
 */

#ifndef NETSURF_DESKTOP_DOWNLOAD_H_
#define NETSURF_DESKTOP_DOWNLOAD_H_

#include "utils/errors.h"

struct gui_window;
struct llcache_handle;

/** Type of a download context */
typedef struct download_context download_context;

/**
 * Create a download context
 *
 * \param llcache  Low-level cache handle for download
 * \param parent   Parent window, for UI
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * This must only be called by the core browser window fetch infrastructure.
 * Ownership of the download context object created is passed to the frontend.
 */
nserror download_context_create(struct llcache_handle *llcache,
		struct gui_window *parent);

/**
 * Destroy a download context
 *
 * \param ctx  Context to destroy
 *
 * Called by the frontend when it has finished with a download context
 */
void download_context_destroy(download_context *ctx);

/**
 * Abort a download fetch
 *
 * \param ctx  Context to abort
 *
 * Called by the frontend to abort a download.
 * The context must be destroyed independently.
 */
void download_context_abort(download_context *ctx);

/**
 * Retrieve the URL for a download
 *
 * \param ctx  Context to retrieve URL from
 * \return URL string
 */
const char *download_context_get_url(const download_context *ctx);

/**
 * Retrieve the MIME type for a download
 *
 * \param ctx  Context to retrieve MIME type from
 * \return MIME type string
 */
const char *download_context_get_mime_type(const download_context *ctx);

/**
 * Retrieve total byte length of download
 *
 * \param ctx  Context to retrieve byte length from
 * \return Total length, in bytes, or 0 if unknown
 */
unsigned long download_context_get_total_length(const download_context *ctx);

/**
 * Retrieve the filename for a download
 *
 * \param ctx  Context to retrieve filename from
 * \return Filename string
 */
const char *download_context_get_filename(const download_context *ctx);

#endif
