/*
 * Copyright 2006,2007 Daniel Silverstone <dsilvers@digital-scurf.org>
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
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
 * Fetching of data from a URL (implementation).
 *
 * Active fetches are held in the circular linked list ::fetch_ring. There may
 * be at most ::option_max_fetchers_per_host active requests per Host: header.
 * There may be at most ::option_max_fetchers active requests overall. Inactive
 * fetchers are stored in the ::queue_ring waiting for use.
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <libwapcaplet/libwapcaplet.h>

#include "utils/config.h"
#include "content/fetch.h"
#include "content/fetchers/resource.h"
#include "content/fetchers/about.h"
#include "content/fetchers/curl.h"
#include "content/fetchers/data.h"
#include "content/fetchers/file.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "utils/corestrings.h"
#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsurl.h"
#include "utils/utils.h"
#include "utils/ring.h"

/* Define this to turn on verbose fetch logging */
#undef DEBUG_FETCH_VERBOSE

bool fetch_active;	/**< Fetches in progress, please call fetch_poll(). */

/** Information about a fetcher for a given scheme. */
typedef struct scheme_fetcher_s {
	lwc_string *scheme_name;		/**< The scheme. */
	fetcher_can_fetch can_fetch;		/**< Ensure an URL can be fetched. */
	fetcher_setup_fetch setup_fetch;	/**< Set up a fetch. */
	fetcher_start_fetch start_fetch;	/**< Start a fetch. */
	fetcher_abort_fetch abort_fetch;	/**< Abort a fetch. */
	fetcher_free_fetch free_fetch;		/**< Free a fetch. */
	fetcher_poll_fetcher poll_fetcher;	/**< Poll this fetcher. */
	fetcher_finalise finaliser;		/**< Clean up this fetcher. */
	int refcount;				/**< When zero, clean up the fetcher. */
	struct scheme_fetcher_s *next_fetcher;	/**< Next fetcher in the list. */
	struct scheme_fetcher_s *prev_fetcher;  /**< Prev fetcher in the list. */
} scheme_fetcher;

static scheme_fetcher *fetchers = NULL;

/** Information for a single fetch. */
struct fetch {
	fetch_callback callback;/**< Callback function. */
	nsurl *url;		/**< URL. */
	nsurl *referer;		/**< Referer URL. */
	bool send_referer;	/**< Valid to send the referer */
	bool verifiable;	/**< Transaction is verifiable */
	void *p;		/**< Private data for callback. */
	lwc_string *host;	/**< Host part of URL, interned */
	long http_code;		/**< HTTP response code, or 0. */
	scheme_fetcher *ops;	/**< Fetcher operations for this fetch,
				   NULL if not set. */
	void *fetcher_handle;	/**< The handle for the fetcher. */
	bool fetch_is_active;	/**< This fetch is active. */
	struct fetch *r_prev;	/**< Previous active fetch in ::fetch_ring. */
	struct fetch *r_next;	/**< Next active fetch in ::fetch_ring. */
};

static struct fetch *fetch_ring = 0;	/**< Ring of active fetches. */
static struct fetch *queue_ring = 0;	/**< Ring of queued fetches */

#define fetch_ref_fetcher(F) F->refcount++

/******************************************************************************
 * fetch internals							      *
 ******************************************************************************/

static void fetch_unref_fetcher(scheme_fetcher *fetcher)
{
	if (--fetcher->refcount == 0) {
		fetcher->finaliser(fetcher->scheme_name);
		lwc_string_unref(fetcher->scheme_name);
		if (fetcher == fetchers) {
			fetchers = fetcher->next_fetcher;
			if (fetchers)
				fetchers->prev_fetcher = NULL;
		} else {
			fetcher->prev_fetcher->next_fetcher =
				fetcher->next_fetcher;
			if (fetcher->next_fetcher != NULL)
				fetcher->next_fetcher->prev_fetcher =
					fetcher->prev_fetcher;
		}
		free(fetcher);
	}
}

/**
 * Dispatch a single job
 */
static bool fetch_dispatch_job(struct fetch *fetch)
{
	RING_REMOVE(queue_ring, fetch);
#ifdef DEBUG_FETCH_VERBOSE
	LOG(("Attempting to start fetch %p, fetcher %p, url %s", fetch,
	     fetch->fetcher_handle, nsurl_access(fetch->url)));
#endif
	if (!fetch->ops->start_fetch(fetch->fetcher_handle)) {
		RING_INSERT(queue_ring, fetch); /* Put it back on the end of the queue */
		return false;
	} else {
		RING_INSERT(fetch_ring, fetch);
		fetch->fetch_is_active = true;
		return true;
	}
}

/**
 * Choose and dispatch a single job. Return false if we failed to dispatch
 * anything.
 *
 * We don't check the overall dispatch size here because we're not called unless
 * there is room in the fetch queue for us.
 */
static bool fetch_choose_and_dispatch(void)
{
	bool same_host;
	struct fetch *queueitem;
	queueitem = queue_ring;
	do {
		/* We can dispatch the selected item if there is room in the
		 * fetch ring
		 */
		int countbyhost;
		RING_COUNTBYLWCHOST(struct fetch, fetch_ring, countbyhost,
				    queueitem->host);
		if (countbyhost < nsoption_int(max_fetchers_per_host)) {
			/* We can dispatch this item in theory */
			return fetch_dispatch_job(queueitem);
		}
		/* skip over other items with the same host */
		same_host = true;
		while (same_host == true && queueitem->r_next != queue_ring) {
			if (lwc_string_isequal(queueitem->host,
					       queueitem->r_next->host, &same_host) ==
			    lwc_error_ok && same_host == true) {
				queueitem = queueitem->r_next;
			}
		}
		queueitem = queueitem->r_next;
	} while (queueitem != queue_ring);
	return false;
}

/**
 * Dispatch as many jobs as we have room to dispatch.
 */
static void fetch_dispatch_jobs(void)
{
	int all_active, all_queued;
#ifdef DEBUG_FETCH_VERBOSE
	struct fetch *q;
	struct fetch *f;
#endif

	if (!queue_ring)
		return; /* Nothing to do, the queue is empty */
	RING_GETSIZE(struct fetch, queue_ring, all_queued);
	RING_GETSIZE(struct fetch, fetch_ring, all_active);

#ifdef DEBUG_FETCH_VERBOSE
	LOG(("queue_ring %i, fetch_ring %i", all_queued, all_active));

	q = queue_ring;
	if (q) {
		do {
			LOG(("queue_ring: %s", q->url));
			q = q->r_next;
		} while (q != queue_ring);
	}
	f = fetch_ring;
	if (f) {
		do {
			LOG(("fetch_ring: %s", f->url));
			f = f->r_next;
		} while (f != fetch_ring);
	}
#endif

	while ( all_queued && all_active < nsoption_int(max_fetchers) ) {
		/*LOG(("%d queued, %d fetching", all_queued, all_active));*/
		if (fetch_choose_and_dispatch()) {
			all_queued--;
			all_active++;
		} else {
			/* Either a dispatch failed or we ran out. Just stop */
			break;
		}
	}
	fetch_active = (all_active > 0);
#ifdef DEBUG_FETCH_VERBOSE
	LOG(("Fetch ring is now %d elements.", all_active));
	LOG(("Queue ring is now %d elements.", all_queued));
#endif
}

/******************************************************************************
 * Public API								      *
 ******************************************************************************/

/* exported interface documented in content/fetch.h */
nserror fetch_init(void)
{
	fetch_curl_register();
	fetch_data_register();
	fetch_file_register();
	fetch_resource_register();
	fetch_about_register();

	fetch_active = false;

	return NSERROR_OK;
}

/* exported interface documented in content/fetch.h */
void fetch_quit(void)
{
	while (fetchers != NULL) {
		if (fetchers->refcount != 1) {
			LOG(("Fetcher for scheme %s still active?!",
			     lwc_string_data(fetchers->scheme_name)));
			/* We shouldn't do this, but... */
			fetchers->refcount = 1;
		}
		fetch_unref_fetcher(fetchers);
	}
}

/* exported interface documented in content/fetch.h */
bool fetch_add_fetcher(lwc_string *scheme,
		       fetcher_initialise initialiser,
		       fetcher_can_fetch can_fetch,
		       fetcher_setup_fetch setup_fetch,
		       fetcher_start_fetch start_fetch,
		       fetcher_abort_fetch abort_fetch,
		       fetcher_free_fetch free_fetch,
		       fetcher_poll_fetcher poll_fetcher,
		       fetcher_finalise finaliser)
{
	scheme_fetcher *new_fetcher;
	if (!initialiser(scheme))
		return false;
	new_fetcher = malloc(sizeof(scheme_fetcher));
	if (new_fetcher == NULL) {
		finaliser(scheme);
		return false;
	}
	new_fetcher->scheme_name = scheme;
	new_fetcher->refcount = 0;
	new_fetcher->can_fetch = can_fetch;
	new_fetcher->setup_fetch = setup_fetch;
	new_fetcher->start_fetch = start_fetch;
	new_fetcher->abort_fetch = abort_fetch;
	new_fetcher->free_fetch = free_fetch;
	new_fetcher->poll_fetcher = poll_fetcher;
	new_fetcher->finaliser = finaliser;
	new_fetcher->next_fetcher = fetchers;
	fetchers = new_fetcher;
	fetch_ref_fetcher(new_fetcher);

	return true;
}

/* exported interface documented in content/fetch.h */
struct fetch * fetch_start(nsurl *url, nsurl *referer,
			   fetch_callback callback,
			   void *p, bool only_2xx, const char *post_urlenc,
			   const struct fetch_multipart_data *post_multipart,
			   bool verifiable, bool downgrade_tls,
			   const char *headers[])
{
	struct fetch *fetch;
	scheme_fetcher *fetcher = fetchers;
	lwc_string *scheme;
	bool match;

	fetch = malloc(sizeof (*fetch));
	if (fetch == NULL)
		return NULL;

	/* The URL we're fetching must have a scheme */
	scheme = nsurl_get_component(url, NSURL_SCHEME);
	assert(scheme != NULL);

#ifdef DEBUG_FETCH_VERBOSE
	LOG(("fetch %p, url '%s'", fetch, nsurl_access(url)));
#endif

	/* construct a new fetch structure */
	fetch->callback = callback;
	fetch->url = nsurl_ref(url);
	fetch->verifiable = verifiable;
	fetch->p = p;
	fetch->http_code = 0;
	fetch->r_prev = NULL;
	fetch->r_next = NULL;
	fetch->referer = NULL;
	fetch->send_referer = false;
	fetch->fetcher_handle = NULL;
	fetch->ops = NULL;
	fetch->fetch_is_active = false;
	fetch->host = nsurl_get_component(url, NSURL_HOST);

	if (referer != NULL) {
		lwc_string *ref_scheme;
		fetch->referer = nsurl_ref(referer);

		ref_scheme = nsurl_get_component(referer, NSURL_SCHEME);
		/* Not a problem if referer has no scheme */

		/* Determine whether to send the Referer header */
		if (nsoption_bool(send_referer) && ref_scheme != NULL) {
			/* User permits us to send the header
			 * Only send it if:
			 *    1) The fetch and referer schemes match
			 * or 2) The fetch is https and the referer is http
			 *
			 * This ensures that referer information is only sent
			 * across schemes in the special case of an https
			 * request from a page served over http. The inverse
			 * (https -> http) should not send the referer (15.1.3)
			 */
			bool match1;
			bool match2;
			if (lwc_string_isequal(scheme, ref_scheme,
					       &match) != lwc_error_ok) {
				match = false;
			}
			if (lwc_string_isequal(scheme, corestring_lwc_https,
					       &match1) != lwc_error_ok) {
				match1 = false;
			}
			if (lwc_string_isequal(ref_scheme, corestring_lwc_http,
					       &match2) != lwc_error_ok) {
				match2= false;
			}
			if (match == true || (match1 == true && match2 == true))
				fetch->send_referer = true;
		}
		if (ref_scheme != NULL)
			lwc_string_unref(ref_scheme);
	}

	/* Pick the scheme ops */
	while (fetcher) {
		if ((lwc_string_isequal(fetcher->scheme_name, scheme,
					&match) == lwc_error_ok) && (match == true)) {
			fetch->ops = fetcher;
			break;
		}
		fetcher = fetcher->next_fetcher;
	}

	if (fetch->ops == NULL)
		goto failed;

	/* Got a scheme fetcher, try and set up the fetch */
	fetch->fetcher_handle = fetch->ops->setup_fetch(fetch, url,
							only_2xx, downgrade_tls,
							post_urlenc, post_multipart,
							headers);

	if (fetch->fetcher_handle == NULL)
		goto failed;

	/* Rah, got it, so ref the fetcher. */
	fetch_ref_fetcher(fetch->ops);

	/* these aren't needed past here */
	lwc_string_unref(scheme);

	/* Dump us in the queue and ask the queue to run. */
	RING_INSERT(queue_ring, fetch);
	fetch_dispatch_jobs();

	return fetch;

failed:
	lwc_string_unref(scheme);

	if (fetch->host != NULL)
		lwc_string_unref(fetch->host);
	if (fetch->url != NULL)
		nsurl_unref(fetch->url);
	if (fetch->referer != NULL)
		nsurl_unref(fetch->referer);

	free(fetch);

	return NULL;
}

/* exported interface documented in content/fetch.h */
void fetch_abort(struct fetch *f)
{
	assert(f);
#ifdef DEBUG_FETCH_VERBOSE
	LOG(("fetch %p, fetcher %p, url '%s'", f, f->fetcher_handle,
	     nsurl_access(f->url)));
#endif
	f->ops->abort_fetch(f->fetcher_handle);
}

/* exported interface documented in content/fetch.h */
void fetch_free(struct fetch *f)
{
#ifdef DEBUG_FETCH_VERBOSE
	LOG(("Freeing fetch %p, fetcher %p", f, f->fetcher_handle));
#endif
	f->ops->free_fetch(f->fetcher_handle);
	fetch_unref_fetcher(f->ops);
	nsurl_unref(f->url);
	if (f->referer != NULL)
		nsurl_unref(f->referer);
	if (f->host != NULL)
		lwc_string_unref(f->host);
	free(f);
}

/* exported interface documented in content/fetch.h */
void fetch_poll(void)
{
	scheme_fetcher *fetcher = fetchers;
	scheme_fetcher *next_fetcher;

	fetch_dispatch_jobs();

	if (!fetch_active)
		return; /* No point polling, there's no fetch active. */
	while (fetcher != NULL) {
		next_fetcher = fetcher->next_fetcher;
		if (fetcher->poll_fetcher != NULL) {
			/* LOG(("Polling fetcher for %s",
			   lwc_string_data(fetcher->scheme_name))); */
			fetcher->poll_fetcher(fetcher->scheme_name);
		}
		fetcher = next_fetcher;
	}
}

/* exported interface documented in content/fetch.h */
bool fetch_can_fetch(const nsurl *url)
{
	scheme_fetcher *fetcher = fetchers;
	bool match;
	lwc_string *scheme = nsurl_get_component(url, NSURL_SCHEME);

	while (fetcher != NULL) {
		if (lwc_string_isequal(fetcher->scheme_name, scheme, &match) == lwc_error_ok && match == true) {
			break;
		}

		fetcher = fetcher->next_fetcher;
	}

	lwc_string_unref(scheme);

	return fetcher == NULL ? false : fetcher->can_fetch(url);
}

/* exported interface documented in content/fetch.h */
void fetch_change_callback(struct fetch *fetch,
			   fetch_callback callback,
			   void *p)
{
	assert(fetch);
	fetch->callback = callback;
	fetch->p = p;
}

/* exported interface documented in content/fetch.h */
long fetch_http_code(struct fetch *fetch)
{
	return fetch->http_code;
}

/* exported interface documented in content/fetch.h */
bool fetch_get_verifiable(struct fetch *fetch)
{
	assert(fetch);

	return fetch->verifiable;
}

/* exported interface documented in content/fetch.h */
struct fetch_multipart_data *
fetch_multipart_data_clone(const struct fetch_multipart_data *list)
{
	struct fetch_multipart_data *clone, *last = NULL;
	struct fetch_multipart_data *result = NULL;

	for (; list != NULL; list = list->next) {
		clone = malloc(sizeof(struct fetch_multipart_data));
		if (clone == NULL) {
			if (result != NULL)
				fetch_multipart_data_destroy(result);

			return NULL;
		}

		clone->file = list->file;

		clone->name = strdup(list->name);
		if (clone->name == NULL) {
			free(clone);
			if (result != NULL)
				fetch_multipart_data_destroy(result);

			return NULL;
		}

		clone->value = strdup(list->value);
		if (clone->value == NULL) {
			free(clone->name);
			free(clone);
			if (result != NULL)
				fetch_multipart_data_destroy(result);

			return NULL;
		}

		if (clone->file) {
			clone->rawfile = strdup(list->rawfile);
			if (clone->rawfile == NULL) {
				free(clone->value);
				free(clone->name);
				free(clone);
				if (result != NULL)
					fetch_multipart_data_destroy(result);

				return NULL;
			}
		} else {
			clone->rawfile = NULL;
		}

		clone->next = NULL;

		if (result == NULL)
			result = clone;
		else
			last->next = clone;

		last = clone;
	}

	return result;
}

/* exported interface documented in content/fetch.h */
void fetch_multipart_data_destroy(struct fetch_multipart_data *list)
{
	struct fetch_multipart_data *next;

	for (; list != NULL; list = next) {
		next = list->next;
		free(list->name);
		free(list->value);
		if (list->file) {
			LOG(("Freeing rawfile: %s", list->rawfile));
			free(list->rawfile);
		}
		free(list);
	}
}

/* exported interface documented in content/fetch.h */
void
fetch_send_callback(const fetch_msg *msg, struct fetch *fetch)
{
	fetch->callback(msg, fetch->p);
}


/* exported interface documented in content/fetch.h */
void fetch_remove_from_queues(struct fetch *fetch)
{
	int all_active;

#ifdef DEBUG_FETCH_VERBOSE
	int all_queued;
	LOG(("Fetch %p, fetcher %p can be freed", fetch, fetch->fetcher_handle));
#endif

	/* Go ahead and free the fetch properly now */
	if (fetch->fetch_is_active) {
		RING_REMOVE(fetch_ring, fetch);
	} else {
		RING_REMOVE(queue_ring, fetch);
	}

	RING_GETSIZE(struct fetch, fetch_ring, all_active);

	fetch_active = (all_active > 0);

#ifdef DEBUG_FETCH_VERBOSE
	LOG(("Fetch ring is now %d elements.", all_active));

	RING_GETSIZE(struct fetch, queue_ring, all_queued);

	LOG(("Queue ring is now %d elements.", all_queued));
#endif
}


/* exported interface documented in content/fetch.h */
void fetch_set_http_code(struct fetch *fetch, long http_code)
{
#ifdef DEBUG_FETCH_VERBOSE
	LOG(("Setting HTTP code to %ld", http_code));
#endif
	fetch->http_code = http_code;
}

/* exported interface documented in content/fetch.h */
const char *fetch_get_referer_to_send(struct fetch *fetch)
{
	if (fetch->send_referer)
		return nsurl_access(fetch->referer);
	return NULL;
}

/* exported interface documented in content/fetch.h */
void fetch_set_cookie(struct fetch *fetch, const char *data)
{
	assert(fetch && data);

	/* If the fetch is unverifiable err on the side of caution and
	 * do not set the cookie */

	if (fetch->verifiable) {
		/* If the transaction's verifiable, we don't require
		 * that the request uri and the parent domain match,
		 * so don't pass in any referer/parent in this case. */
		urldb_set_cookie(data, fetch->url, NULL);
	} else if (fetch->referer != NULL) {
		/* Permit the cookie to be set if the fetch is unverifiable
		 * and the fetch URI domain matches the referer. */
		/** \todo Long-term, this needs to be replaced with a
		 * comparison against the origin fetch URI. In the case
		 * where a nested object requests a fetch, the origin URI
		 * is the nested object's parent URI, whereas the referer
		 * for the fetch will be the nested object's URI. */
		urldb_set_cookie(data, fetch->url, fetch->referer);
	}
}
