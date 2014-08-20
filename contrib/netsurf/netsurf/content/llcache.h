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
 * Low-level resource cache (interface)
 */

#ifndef NETSURF_CONTENT_LLCACHE_H_
#define NETSURF_CONTENT_LLCACHE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utils/errors.h"
#include "utils/nsurl.h"

struct ssl_cert_info;
struct fetch_multipart_data;

/** Handle for low-level cache object */
typedef struct llcache_handle llcache_handle;

/** POST data object for low-level cache requests */
typedef struct {
	enum {
		LLCACHE_POST_URL_ENCODED,
		LLCACHE_POST_MULTIPART
	} type;				/**< Type of POST data */
	union {
		char *urlenc;		/**< URL encoded data */
		struct fetch_multipart_data *multipart; /**< Multipart data */
	} data;				/**< POST data content */
} llcache_post_data;

/** Low-level cache event types */
typedef enum {
	LLCACHE_EVENT_HAD_HEADERS,	/**< Received all headers */
	LLCACHE_EVENT_HAD_DATA,		/**< Received some data */
	LLCACHE_EVENT_DONE,		/**< Finished fetching data */

	LLCACHE_EVENT_ERROR,		/**< An error occurred during fetch */
	LLCACHE_EVENT_PROGRESS,		/**< Fetch progress update */

	LLCACHE_EVENT_REDIRECT		/**< Fetch URL redirect occured */
} llcache_event_type;

/** Low-level cache events */
typedef struct {
	llcache_event_type type;	/**< Type of event */
	union {
		struct {
			const uint8_t *buf;	/**< Buffer of data */
			size_t len;	/**< Length of buffer, in bytes */
		} data;			/**< Received data */
		const char *msg;	/**< Error or progress message */
		struct {
			nsurl *from;	/**< Redirect origin */
			nsurl *to;	/**< Redirect target */
		} redirect;		/**< Fetch URL redirect occured */
	} data;				/**< Event data */
} llcache_event;

/** 
 * Client callback for low-level cache events
 *
 * \param handle  Handle for which event is issued
 * \param event   Event data
 * \param pw      Pointer to client-specific data
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
typedef nserror (*llcache_handle_callback)(llcache_handle *handle, 
		const llcache_event *event, void *pw);

/** Flags for low-level cache object retrieval */
enum llcache_retrieve_flag {
	/* Note: We're permitted a maximum of 16 flags which must reside in the
	 * bottom 16 bits of the flags word. See hlcache.h for further details. 
	 */
	/** Force a new fetch */
	LLCACHE_RETRIEVE_FORCE_FETCH    = (1 << 0), 
	/** Requested URL was verified */
	LLCACHE_RETRIEVE_VERIFIABLE     = (1 << 1), 
	/**< No error pages */
	LLCACHE_RETRIEVE_NO_ERROR_PAGES = (1 << 2),
	/**< Stream data (implies that object is not cacheable) */
	LLCACHE_RETRIEVE_STREAM_DATA    = (1 << 3)
};

/** Low-level cache query types */
typedef enum {
	LLCACHE_QUERY_AUTH,		/**< Need authentication details */
	LLCACHE_QUERY_REDIRECT,		/**< Need permission to redirect */
	LLCACHE_QUERY_SSL		/**< SSL chain needs inspection */
} llcache_query_type;

/** Low-level cache query */
typedef struct {
	llcache_query_type type;	/**< Type of query */

	nsurl *url;			/**< URL being fetched */

	union {
		struct {
			const char *realm;	/**< Authentication realm */
		} auth;

		struct {
			const char *target;	/**< Redirect target */
		} redirect;

		struct {
			const struct ssl_cert_info *certs;
			size_t num;		/**< Number of certs in chain */
		} ssl;
	} data;
} llcache_query;

/**
 * Response handler for fetch-related queries
 *
 * \param proceed  Whether to proceed with the fetch or not
 * \param cbpw     Opaque value provided to llcache_query_callback
 * \return NSERROR_OK on success, appropriate error otherwise
 */
typedef nserror (*llcache_query_response)(bool proceed, void *cbpw);

/**
 * Callback to handle fetch-related queries
 *
 * \param query  Object containing details of query
 * \param pw     Pointer to callback-specific data
 * \param cb     Callback that client should call once query is satisfied
 * \param cbpw   Opaque value to pass into \a cb
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \note This callback should return immediately. Once a suitable answer to 
 *       the query has been obtained, the provided response callback should be 
 *       called. This is intended to be an entirely asynchronous process.
 */
typedef nserror (*llcache_query_callback)(const llcache_query *query, void *pw,
		llcache_query_response cb, void *cbpw);

/**
 * Initialise the low-level cache
 *
 * \param cb  Query handler
 * \param pw  Pointer to query handler data
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
nserror llcache_initialise(llcache_query_callback cb, void *pw, uint32_t llcache_limit);

/**
 * Finalise the low-level cache
 */
void llcache_finalise(void);

/**
 * Cause the low-level cache to emit any pending notifications.
 *
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
nserror llcache_poll(void);

/**
 * Cause the low-level cache to attempt to perform cleanup.  No
 * guarantees are made as to whether or not cleanups will take
 * place and what, if any, space savings will be made.
 */
void llcache_clean(void);

/**
 * Retrieve a handle for a low-level cache object
 *
 * \param url      URL of the object to fetch
 * \param flags    Object retrieval flags
 * \param referer  Referring URL, or NULL if none
 * \param post     POST data, or NULL for a GET request
 * \param cb       Client callback for events
 * \param pw       Pointer to client-specific data
 * \param result   Pointer to location to recieve cache handle
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_handle_retrieve(nsurl *url, uint32_t flags,
		nsurl *referer, const llcache_post_data *post,
		llcache_handle_callback cb, void *pw,
		llcache_handle **result);

/**
 * Change the callback associated with a low-level cache handle
 *
 * \param handle  Handle to change callback of
 * \param cb      New callback
 * \param pw      Client data for new callback
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_handle_change_callback(llcache_handle *handle,
		llcache_handle_callback cb, void *pw);

/**
 * Release a low-level cache handle
 *
 * \param handle  Handle to release
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_handle_release(llcache_handle *handle);

/**
 * Clone a low-level cache handle, producing a new handle to
 * the same fetch/content.
 *
 * \param handle  Handle to clone
 * \param result  Pointer to location to receive cloned handle
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_handle_clone(llcache_handle *handle, llcache_handle **result);

/**
 * Abort a low-level fetch, informing all users of this action.
 *
 * \param handle  Handle to abort
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_handle_abort(llcache_handle *handle);

/**
 * Force a low-level cache handle into streaming mode
 *
 * \param handle  Handle to stream
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_handle_force_stream(llcache_handle *handle);

/**
 * Invalidate cache data for a low-level cache object
 *
 * \param handle  Handle to invalidate
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror llcache_handle_invalidate_cache_data(llcache_handle *handle);

/**
 * Retrieve the post-redirect URL of a low-level cache object
 *
 * \param handle  Handle to retrieve URL from
 * \return Post-redirect URL of cache object
 */
nsurl *llcache_handle_get_url(const llcache_handle *handle);

/**
 * Retrieve source data of a low-level cache object
 *
 * \param handle  Handle to retrieve source data from
 * \param size    Pointer to location to receive byte length of data
 * \return Pointer to source data
 */
const uint8_t *llcache_handle_get_source_data(const llcache_handle *handle,
		size_t *size);

/**
 * Retrieve a header value associated with a low-level cache object
 *
 * \param handle  Handle to retrieve header from
 * \param key     Header name
 * \return Header value, or NULL if header does not exist
 *
 * \todo Make the key an enumeration, to avoid needless string comparisons
 * \todo Forcing the client to parse the header value seems wrong. 
 *       Better would be to return the actual value part and an array of 
 *       key-value pairs for any additional parameters.
 * \todo Deal with multiple headers of the same key (e.g. Set-Cookie)
 */
const char *llcache_handle_get_header(const llcache_handle *handle, 
		const char *key);

/**
 * Determine if the same underlying object is referenced by the given handles
 *
 * \param a  First handle
 * \param b  Second handle
 * \return True if handles reference the same object, false otherwise
 */
bool llcache_handle_references_same_object(const llcache_handle *a, 
		const llcache_handle *b);

#endif
