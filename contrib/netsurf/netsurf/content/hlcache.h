/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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
 * High-level resource cache (interface)
 */

#ifndef NETSURF_CONTENT_HLCACHE_H_
#define NETSURF_CONTENT_HLCACHE_H_

#include "content/content.h"
#include "content/llcache.h"
#include "utils/errors.h"
#include "utils/nsurl.h"

/** High-level cache handle */
typedef struct hlcache_handle hlcache_handle;

/** Context for retrieving a child object */
typedef struct hlcache_child_context {
 	const char *charset;		/**< Charset of parent */
 	bool quirks;			/**< Whether parent is quirky */
} hlcache_child_context;

/** High-level cache event */
typedef struct {
	content_msg type;		/**< Event type */
	union content_msg_data data;	/**< Event data */
} hlcache_event;

struct hlcache_parameters {
	llcache_query_callback cb; /**< Query handler for llcache */
	void *cb_ctx; /**< Pointer to llcache query handler data */

	/** How frequently the background cache clean process is run (ms) */
	unsigned int bg_clean_time;

	/** The target upper bound for the cache size */
	size_t limit;

	/** The hysteresis allowed round the target size */
	size_t hysteresis;

};

/**
 * Client callback for high-level cache events
 *
 * \param handle  Handle to object generating event
 * \param event   Event data
 * \param pw      Pointer to client-specific data
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
typedef nserror (*hlcache_handle_callback)(hlcache_handle *handle,
		const hlcache_event *event, void *pw); 

/** Flags for high-level cache object retrieval */
enum hlcache_retrieve_flag {
	/* Note: low-level cache retrieval flags occupy the bottom 16 bits of 
	 * the flags word. High-level cache flags occupy the top 16 bits. 
	 * To avoid confusion, high-level flags are allocated from bit 31 down. 
	 */
	/** It's permitted to convert this request into a download */
	HLCACHE_RETRIEVE_MAY_DOWNLOAD = (1 << 31),
	/* Permit content-type sniffing */
	HLCACHE_RETRIEVE_SNIFF_TYPE   = (1 << 30)
};

/**
 * Initialise the high-level cache, preparing the llcache also.
 *
 * \param hlcache_parameters Settings to initialise cache with  
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
nserror hlcache_initialise(const struct hlcache_parameters *hlcache_parameters);

/**
 * Stop the high-level cache periodic functionality so that the
 * exit sequence can run.
 */
void hlcache_stop(void);

/**
 * Finalise the high-level cache, destroying any remaining contents
 */
void hlcache_finalise(void);

/**
 * Drive the low-level cache poll loop, and attempt to clean the cache.
 * No guarantee is made about what, if any, cache cleaning will occur.
 *
 * \return NSERROR_OK
 */
nserror hlcache_poll(void);

/**
 * Retrieve a high-level cache handle for an object
 *
 * \param url             URL of the object to retrieve handle for
 * \param flags           Object retrieval flags
 * \param referer         Referring URL, or NULL if none
 * \param post            POST data, or NULL for a GET request
 * \param cb              Callback to handle object events
 * \param pw              Pointer to client-specific data for callback
 * \param child           Child retrieval context, or NULL for top-level content
 * \param accepted_types  Bitmap of acceptable content types
 * \param result          Pointer to location to recieve cache handle
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * Child contents are keyed on the tuple < URL, quirks >.
 * The quirks field is ignored for child contents whose behaviour is not
 * affected by quirks mode.
 *
 * \todo The above rules should be encoded in the handler_map.
 *
 * \todo Is there any way to sensibly reduce the number of parameters here?
 */
nserror hlcache_handle_retrieve(nsurl *url, uint32_t flags,
		nsurl *referer, llcache_post_data *post,
		hlcache_handle_callback cb, void *pw,
		hlcache_child_context *child, 
		content_type accepted_types, hlcache_handle **result);

/**
 * Release a high-level cache handle
 *
 * \param handle  Handle to release
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror hlcache_handle_release(hlcache_handle *handle);

/**
 * Abort a high-level cache fetch
 *
 * \param handle  Handle to abort
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror hlcache_handle_abort(hlcache_handle *handle);

/**
 * Replace a high-level cache handle's callback
 *
 * \param handle  Handle to replace callback of
 * \param cb      New callback routine
 * \param pw      Private data for callback
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror hlcache_handle_replace_callback(hlcache_handle *handle,
		hlcache_handle_callback cb, void *pw);

/**
 * Retrieve a content object from a cache handle
 *
 * \param handle  Cache handle to dereference
 * \return Pointer to content object, or NULL if there is none
 *
 * \todo This may not be correct. Ideally, the client should never need to 
 * directly access a content object. It may, therefore, be better to provide a 
 * bunch of veneers here that take a hlcache_handle and invoke the 
 * corresponding content_ API. If there's no content object associated with the
 * hlcache_handle (e.g. because the source data is still being fetched, so it 
 * doesn't exist yet), then these veneers would behave as a NOP. The important 
 * thing being that the client need not care about this possibility and can 
 * just call the functions with impugnity.
 */
struct content *hlcache_handle_get_content(const hlcache_handle *handle);

/**
 * Clone a high level cache handle.
 *
 * \param handle The handle to clone.
 * \param result The cloned handle.
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 */
nserror hlcache_handle_clone(hlcache_handle *handle, hlcache_handle **result);

/**
 * Retrieve the URL associated with a high level cache handle
 *
 * \param handle  The handle to inspect
 * \return  Pointer to URL.
 */
nsurl *hlcache_handle_get_url(const hlcache_handle *handle);

#endif
