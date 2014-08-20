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
 * High-level resource cache (implementation)
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "desktop/gui_factory.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/mimesniff.h"
#include "utils/http.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/ring.h"
#include "utils/url.h"
#include "utils/utils.h"

typedef struct hlcache_entry hlcache_entry;
typedef struct hlcache_retrieval_ctx hlcache_retrieval_ctx;

/** High-level cache retrieval context */
struct hlcache_retrieval_ctx {
	struct hlcache_retrieval_ctx *r_prev; /**< Previous retrieval context in the ring */
	struct hlcache_retrieval_ctx *r_next; /**< Next retrieval context in the ring */

	llcache_handle *llcache;	/**< Low-level cache handle */

	hlcache_handle *handle;		/**< High-level handle for object */

	uint32_t flags;			/**< Retrieval flags */

	content_type accepted_types;	/**< Accepted types */

	hlcache_child_context child;	/**< Child context */

	bool migrate_target;		/**< Whether this context is the migration target */
};

/** High-level cache handle */
struct hlcache_handle {
	hlcache_entry *entry;		/**< Pointer to cache entry */

	hlcache_handle_callback cb;	/**< Client callback */
	void *pw;			/**< Client data */
};

/** Entry in high-level cache */
struct hlcache_entry {
	struct content *content;	/**< Pointer to associated content */

	hlcache_entry *next;		/**< Next sibling */
	hlcache_entry *prev;		/**< Previous sibling */
};

/** Current state of the cache.
 *
 * Global state of the cache.
 */
struct hlcache_s {
	struct hlcache_parameters params;

	/** List of cached content objects */
	hlcache_entry *content_list;

	/** Ring of retrieval contexts */
	hlcache_retrieval_ctx *retrieval_ctx_ring;

	/* statsistics */
	unsigned int hit_count;
	unsigned int miss_count;
};

/** high level cache state */
static struct hlcache_s *hlcache = NULL;


/******************************************************************************
 * High-level cache internals						      *
 ******************************************************************************/


/**
 * Attempt to clean the cache
 */
static void hlcache_clean(void *ignored)
{
	hlcache_entry *entry, *next;

	for (entry = hlcache->content_list; entry != NULL; entry = next) {
		next = entry->next;

		if (entry->content == NULL)
			continue;

		if (content__get_status(entry->content) ==
				CONTENT_STATUS_LOADING)
			continue;

		if (content_count_users(entry->content) != 0)
			continue;

		/** \todo This is over-zealous: all unused contents
		 * will be immediately destroyed. Ideally, we want to
		 * purge all unused contents that are using stale
		 * source data, and enough fresh contents such that
		 * the cache fits in the configured cache size limit.
		 */

		/* Remove entry from cache */
		if (entry->prev == NULL)
			hlcache->content_list = entry->next;
		else
			entry->prev->next = entry->next;

		if (entry->next != NULL)
			entry->next->prev = entry->prev;

		/* Destroy content */
		content_destroy(entry->content);

		/* Destroy entry */
		free(entry);
	}

	/* Attempt to clean the llcache */
	llcache_clean();

	/* Re-schedule ourselves */
	guit->browser->schedule(hlcache->params.bg_clean_time, hlcache_clean, NULL);
}

/**
 * Determine if the specified MIME type is acceptable
 *
 * \param mime_type       MIME type to consider
 * \param accepted_types  Array of acceptable types, or NULL for any
 * \param computed_type	  Pointer to location to receive computed type of object
 * \return True if the type is acceptable, false otherwise
 */
static bool hlcache_type_is_acceptable(lwc_string *mime_type,
		content_type accepted_types, content_type *computed_type)
{
	content_type type;

	type = content_factory_type_from_mime_type(mime_type);

	*computed_type = type;

	return ((accepted_types & type) != 0);
}

/**
 * Veneer between content callback API and hlcache callback API
 *
 * \param c	Content to emit message for
 * \param msg	Message to emit
 * \param data	Data for message
 * \param pw	Pointer to private data (hlcache_handle)
 */
static void hlcache_content_callback(struct content *c, content_msg msg,
		union content_msg_data data, void *pw)
{
	hlcache_handle *handle = pw;
	hlcache_event event;
	nserror error = NSERROR_OK;

	event.type = msg;
	event.data = data;

	if (handle->cb != NULL)
		error = handle->cb(handle, &event, handle->pw);

	if (error != NSERROR_OK)
		LOG(("Error in callback: %d", error));
}

/**
 * Find a content for the high-level cache handle
 *
 * \param ctx             High-level cache retrieval context
 * \param effective_type  Effective MIME type of content
 * \return NSERROR_OK on success,
 *         NSERROR_NEED_DATA on success where data is needed,
 *         appropriate error otherwise
 *
 * \pre handle::state == HLCACHE_HANDLE_NEW
 * \pre Headers must have been received for associated low-level handle
 * \post Low-level handle is either released, or associated with new content
 * \post High-level handle is registered with content
 */
static nserror hlcache_find_content(hlcache_retrieval_ctx *ctx,
		lwc_string *effective_type)
{
	hlcache_entry *entry;
	hlcache_event event;
	nserror error = NSERROR_OK;

	/* Search list of cached contents for a suitable one */
	for (entry = hlcache->content_list; entry != NULL; entry = entry->next) {
		hlcache_handle entry_handle = { entry, NULL, NULL };
		const llcache_handle *entry_llcache;

		if (entry->content == NULL)
			continue;

		/* Ignore contents in the error state */
		if (content_get_status(&entry_handle) == CONTENT_STATUS_ERROR)
			continue;

		/* Ensure that content is shareable */
		if (content_is_shareable(entry->content) == false)
			continue;

		/* Ensure that quirks mode is acceptable */
		if (content_matches_quirks(entry->content,
				ctx->child.quirks) == false)
			continue;

		/* Ensure that content uses same low-level object as
		 * low-level handle */
		entry_llcache = content_get_llcache_handle(entry->content);

		if (llcache_handle_references_same_object(entry_llcache,
				ctx->llcache))
			break;
	}

	if (entry == NULL) {
		/* No existing entry, so need to create one */
		entry = malloc(sizeof(hlcache_entry));
		if (entry == NULL)
			return NSERROR_NOMEM;

		/* Create content using llhandle */
		entry->content = content_factory_create_content(ctx->llcache,
				ctx->child.charset, ctx->child.quirks,
				effective_type);
		if (entry->content == NULL) {
			free(entry);
			return NSERROR_NOMEM;
		}

		/* Insert into cache */
		entry->prev = NULL;
		entry->next = hlcache->content_list;
		if (hlcache->content_list != NULL)
			hlcache->content_list->prev = entry;
		hlcache->content_list = entry;

		/* Signal to caller that we created a content */
		error = NSERROR_NEED_DATA;

		hlcache->miss_count++;
	} else {
		/* Found a suitable content: no longer need low-level handle */
		llcache_handle_release(ctx->llcache);
		hlcache->hit_count++;
	}

	/* Associate handle with content */
	if (content_add_user(entry->content,
			hlcache_content_callback, ctx->handle) == false)
		return NSERROR_NOMEM;

	/* Associate cache entry with handle */
	ctx->handle->entry = entry;

	/* Catch handle up with state of content */
	if (ctx->handle->cb != NULL) {
		content_status status = content_get_status(ctx->handle);

		if (status == CONTENT_STATUS_LOADING) {
			event.type = CONTENT_MSG_LOADING;
			ctx->handle->cb(ctx->handle, &event, ctx->handle->pw);
		} else if (status == CONTENT_STATUS_READY) {
			event.type = CONTENT_MSG_LOADING;
			ctx->handle->cb(ctx->handle, &event, ctx->handle->pw);

			if (ctx->handle->cb != NULL) {
				event.type = CONTENT_MSG_READY;
				ctx->handle->cb(ctx->handle, &event,
						ctx->handle->pw);
			}
		} else if (status == CONTENT_STATUS_DONE) {
			event.type = CONTENT_MSG_LOADING;
			ctx->handle->cb(ctx->handle, &event, ctx->handle->pw);

			if (ctx->handle->cb != NULL) {
				event.type = CONTENT_MSG_READY;
				ctx->handle->cb(ctx->handle, &event,
						ctx->handle->pw);
			}

			if (ctx->handle->cb != NULL) {
				event.type = CONTENT_MSG_DONE;
				ctx->handle->cb(ctx->handle, &event,
						ctx->handle->pw);
			}
		}
	}

	return error;
}

/**
 * Migrate a retrieval context into its final destination content
 *
 * \param ctx             Context to migrate
 * \param effective_type  The effective MIME type of the content, or NULL
 * \return NSERROR_OK on success,
 *         NSERROR_NEED_DATA on success where data is needed,
 *         appropriate error otherwise
 */
static nserror hlcache_migrate_ctx(hlcache_retrieval_ctx *ctx,
		lwc_string *effective_type)
{
	content_type type = CONTENT_NONE;
	nserror error = NSERROR_OK;

	ctx->migrate_target = true;

	if (effective_type != NULL &&
			hlcache_type_is_acceptable(effective_type,
			ctx->accepted_types, &type)) {
		error = hlcache_find_content(ctx, effective_type);
		if (error != NSERROR_OK && error != NSERROR_NEED_DATA) {
			if (ctx->handle->cb != NULL) {
				hlcache_event hlevent;

				hlevent.type = CONTENT_MSG_ERROR;
				hlevent.data.error = messages_get("MiscError");

				ctx->handle->cb(ctx->handle, &hlevent,
						ctx->handle->pw);
			}

			llcache_handle_abort(ctx->llcache);
			llcache_handle_release(ctx->llcache);
		}
	} else if (type == CONTENT_NONE &&
			(ctx->flags & HLCACHE_RETRIEVE_MAY_DOWNLOAD)) {
		/* Unknown type, and we can download, so convert */
		llcache_handle_force_stream(ctx->llcache);

		if (ctx->handle->cb != NULL) {
			hlcache_event hlevent;

			hlevent.type = CONTENT_MSG_DOWNLOAD;
			hlevent.data.download = ctx->llcache;

			ctx->handle->cb(ctx->handle, &hlevent,
					ctx->handle->pw);
		}

		/* Ensure caller knows we need data */
		error = NSERROR_NEED_DATA;
	} else {
		/* Unacceptable type: report error */
		if (ctx->handle->cb != NULL) {
			hlcache_event hlevent;

			hlevent.type = CONTENT_MSG_ERROR;
			hlevent.data.error = messages_get("UnacceptableType");

			ctx->handle->cb(ctx->handle, &hlevent,
					ctx->handle->pw);
		}

		llcache_handle_abort(ctx->llcache);
		llcache_handle_release(ctx->llcache);
	}

	ctx->migrate_target = false;

	/* No longer require retrieval context */
	RING_REMOVE(hlcache->retrieval_ctx_ring, ctx);
	free((char *) ctx->child.charset);
	free(ctx);

	return error;
}

/**
 * Handler for low-level cache events
 *
 * \param handle  Handle for which event is issued
 * \param event	  Event data
 * \param pw	  Pointer to client-specific data
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror hlcache_llcache_callback(llcache_handle *handle,
		const llcache_event *event, void *pw)
{
	hlcache_retrieval_ctx *ctx = pw;
	lwc_string *effective_type = NULL;
	nserror error;

	assert(ctx->llcache == handle);

	switch (event->type) {
	case LLCACHE_EVENT_HAD_HEADERS:
		error = mimesniff_compute_effective_type(handle, NULL, 0,
				ctx->flags & HLCACHE_RETRIEVE_SNIFF_TYPE,
				ctx->accepted_types == CONTENT_IMAGE,
				&effective_type);
		if (error == NSERROR_OK || error == NSERROR_NOT_FOUND) {
			/* If the sniffer was successful or failed to find
			 * a Content-Type header when sniffing was
			 * prohibited, we must migrate the retrieval context. */
			error = hlcache_migrate_ctx(ctx, effective_type);

			if (effective_type != NULL)
				lwc_string_unref(effective_type);
		}

		/* No need to report that we need data:
		 * we'll get some anyway if there is any */
		if (error == NSERROR_NEED_DATA)
			error = NSERROR_OK;

		return error;

		break;
	case LLCACHE_EVENT_HAD_DATA:
		error = mimesniff_compute_effective_type(handle,
				event->data.data.buf, event->data.data.len,
				ctx->flags & HLCACHE_RETRIEVE_SNIFF_TYPE,
				ctx->accepted_types == CONTENT_IMAGE,
				&effective_type);
		if (error != NSERROR_OK) {
			assert(0 && "MIME sniff failed with data");
		}

		error = hlcache_migrate_ctx(ctx, effective_type);

		lwc_string_unref(effective_type);

		return error;

		break;
	case LLCACHE_EVENT_DONE:
		/* DONE event before we could determine the effective MIME type.
		 */
		error = mimesniff_compute_effective_type(handle,
				NULL, 0, false, false, &effective_type);
		if (error == NSERROR_OK) {
			error = hlcache_migrate_ctx(ctx, effective_type);

			lwc_string_unref(effective_type);

			return error;
		}

		if (ctx->handle->cb != NULL) {
			hlcache_event hlevent;

			hlevent.type = CONTENT_MSG_ERROR;
			hlevent.data.error = messages_get("BadType");

			ctx->handle->cb(ctx->handle, &hlevent, ctx->handle->pw);
		}
		break;
	case LLCACHE_EVENT_ERROR:
		if (ctx->handle->cb != NULL) {
			hlcache_event hlevent;

			hlevent.type = CONTENT_MSG_ERROR;
			hlevent.data.error = event->data.msg;

			ctx->handle->cb(ctx->handle, &hlevent, ctx->handle->pw);
		}
		break;
	case LLCACHE_EVENT_PROGRESS:
		break;
	case LLCACHE_EVENT_REDIRECT:
		if (ctx->handle->cb != NULL) {
			hlcache_event hlevent;

			hlevent.type = CONTENT_MSG_REDIRECT;
			hlevent.data.redirect.from = event->data.redirect.from;
			hlevent.data.redirect.to = event->data.redirect.to;

			ctx->handle->cb(ctx->handle, &hlevent, ctx->handle->pw);
		}
		break;
	}

	return NSERROR_OK;
}


/******************************************************************************
 * Public API								      *
 ******************************************************************************/


nserror
hlcache_initialise(const struct hlcache_parameters *hlcache_parameters)
{
	nserror ret;

	hlcache = calloc(1, sizeof(struct hlcache_s));
	if (hlcache == NULL) {
		return NSERROR_NOMEM;
	}

	ret = llcache_initialise(hlcache_parameters->cb,
				 hlcache_parameters->cb_ctx,
				 hlcache_parameters->limit);
	if (ret != NSERROR_OK) {
		free(hlcache);
		hlcache = NULL;
		return ret;
	}

	hlcache->params = *hlcache_parameters;

	/* Schedule the cache cleanup */
	guit->browser->schedule(hlcache->params.bg_clean_time, hlcache_clean, NULL);

	return NSERROR_OK;
}

/* See hlcache.h for documentation */
void hlcache_stop(void)
{
	/* Remove the hlcache_clean schedule */
	guit->browser->schedule(-1, hlcache_clean, NULL);
}

/* See hlcache.h for documentation */
void hlcache_finalise(void)
{
	uint32_t num_contents, prev_contents;
	hlcache_entry *entry;
	hlcache_retrieval_ctx *ctx, *next;

	/* Obtain initial count of contents remaining */
	for (num_contents = 0, entry = hlcache->content_list;
			entry != NULL; entry = entry->next) {
		num_contents++;
	}

	LOG(("%d contents remain before cache drain", num_contents));

	/* Drain cache */
	do {
		prev_contents = num_contents;

		hlcache_clean(NULL);

		for (num_contents = 0, entry = hlcache->content_list;
				entry != NULL; entry = entry->next) {
			num_contents++;
		}
	} while (num_contents > 0 && num_contents != prev_contents);

	LOG(("%d contents remaining:", num_contents));
	for (entry = hlcache->content_list; entry != NULL; entry = entry->next) {
		hlcache_handle entry_handle = { entry, NULL, NULL };

		if (entry->content != NULL) {
			LOG(("	%p : %s (%d users)", entry,
					nsurl_access(
					hlcache_handle_get_url(&entry_handle)),
					content_count_users(entry->content)));
		} else {
			LOG(("	%p", entry));
		}
	}

	/* Clean up retrieval contexts */
	if (hlcache->retrieval_ctx_ring != NULL) {
		ctx = hlcache->retrieval_ctx_ring;

		do {
			next = ctx->r_next;

			if (ctx->llcache != NULL)
				llcache_handle_release(ctx->llcache);

			if (ctx->handle != NULL)
				free(ctx->handle);

			if (ctx->child.charset != NULL)
				free((char *) ctx->child.charset);

			free(ctx);

			ctx = next;
		} while (ctx != hlcache->retrieval_ctx_ring);

		hlcache->retrieval_ctx_ring = NULL;
	}

	LOG(("hit/miss %d/%d", hlcache->hit_count, hlcache->miss_count));

	free(hlcache);
	hlcache = NULL;

	LOG(("Finalising low-level cache"));
	llcache_finalise();
}

/* See hlcache.h for documentation */
nserror hlcache_poll(void)
{

	llcache_poll();

	return NSERROR_OK;
}

/* See hlcache.h for documentation */
nserror hlcache_handle_retrieve(nsurl *url, uint32_t flags,
		nsurl *referer, llcache_post_data *post,
		hlcache_handle_callback cb, void *pw,
		hlcache_child_context *child,
		content_type accepted_types, hlcache_handle **result)
{
	hlcache_retrieval_ctx *ctx;
	nserror error;

	assert(cb != NULL);

	ctx = calloc(1, sizeof(hlcache_retrieval_ctx));
	if (ctx == NULL)
		return NSERROR_NOMEM;

	ctx->handle = calloc(1, sizeof(hlcache_handle));
	if (ctx->handle == NULL) {
		free(ctx);
		return NSERROR_NOMEM;
	}

	if (child != NULL) {
		if (child->charset != NULL) {
			ctx->child.charset = strdup(child->charset);
			if (ctx->child.charset == NULL) {
				free(ctx->handle);
				free(ctx);
				return NSERROR_NOMEM;
			}
		}
		ctx->child.quirks = child->quirks;
	}

	ctx->flags = flags;
	ctx->accepted_types = accepted_types;

	ctx->handle->cb = cb;
	ctx->handle->pw = pw;

	error = llcache_handle_retrieve(url, flags, referer, post,
			hlcache_llcache_callback, ctx,
			&ctx->llcache);
	if (error != NSERROR_OK) {
		free((char *) ctx->child.charset);
		free(ctx->handle);
		free(ctx);
		return error;
	}

	RING_INSERT(hlcache->retrieval_ctx_ring, ctx);

	*result = ctx->handle;

	return NSERROR_OK;
}

/* See hlcache.h for documentation */
nserror hlcache_handle_release(hlcache_handle *handle)
{
	if (handle->entry != NULL) {
		content_remove_user(handle->entry->content,
				hlcache_content_callback, handle);
	} else {
		RING_ITERATE_START(struct hlcache_retrieval_ctx,
				   hlcache->retrieval_ctx_ring,
				   ictx) {
			if (ictx->handle == handle &&
					ictx->migrate_target == false) {
				/* This is the nascent context for us,
				 * so abort the fetch */
				llcache_handle_abort(ictx->llcache);
				llcache_handle_release(ictx->llcache);
				/* Remove us from the ring */
				RING_REMOVE(hlcache->retrieval_ctx_ring, ictx);
				/* Throw us away */
				free((char *) ictx->child.charset);
				free(ictx);
				/* And stop */
				RING_ITERATE_STOP(hlcache->retrieval_ctx_ring,
						ictx);
			}
		} RING_ITERATE_END(hlcache->retrieval_ctx_ring, ictx);
	}

	handle->cb = NULL;
	handle->pw = NULL;

	free(handle);

	return NSERROR_OK;
}

/* See hlcache.h for documentation */
struct content *hlcache_handle_get_content(const hlcache_handle *handle)
{
	assert(handle != NULL);

	if (handle->entry != NULL)
		return handle->entry->content;

	return NULL;
}

/* See hlcache.h for documentation */
nserror hlcache_handle_abort(hlcache_handle *handle)
{
	struct hlcache_entry *entry = handle->entry;
	struct content *c;

	if (entry == NULL) {
		/* This handle is not yet associated with a cache entry.
		 * The implication is that the fetch for the handle has
		 * not progressed to the point where the entry can be
		 * created. */

		RING_ITERATE_START(struct hlcache_retrieval_ctx,
				   hlcache->retrieval_ctx_ring,
				   ictx) {
			if (ictx->handle == handle &&
					ictx->migrate_target == false) {
				/* This is the nascent context for us,
				 * so abort the fetch */
				llcache_handle_abort(ictx->llcache);
				llcache_handle_release(ictx->llcache);
				/* Remove us from the ring */
				RING_REMOVE(hlcache->retrieval_ctx_ring, ictx);
				/* Throw us away */
				free((char *) ictx->child.charset);
				free(ictx);
				/* And stop */
				RING_ITERATE_STOP(hlcache->retrieval_ctx_ring,
						ictx);
			}
		} RING_ITERATE_END(hlcache->retrieval_ctx_ring, ictx);

		return NSERROR_OK;
	}

	c = entry->content;

	if (content_count_users(c) > 1) {
		/* We are not the only user of 'c' so clone it. */
		struct content *clone = content_clone(c);

		if (clone == NULL)
			return NSERROR_NOMEM;

		entry = calloc(sizeof(struct hlcache_entry), 1);

		if (entry == NULL) {
			content_destroy(clone);
			return NSERROR_NOMEM;
		}

		if (content_add_user(clone,
				hlcache_content_callback, handle) == false) {
			content_destroy(clone);
			free(entry);
			return NSERROR_NOMEM;
		}

		content_remove_user(c, hlcache_content_callback, handle);

		entry->content = clone;
		handle->entry = entry;
		entry->prev = NULL;
		entry->next = hlcache->content_list;
		if (hlcache->content_list != NULL)
			hlcache->content_list->prev = entry;
		hlcache->content_list = entry;

		c = clone;
	}

	return content_abort(c);
}

/* See hlcache.h for documentation */
nserror hlcache_handle_replace_callback(hlcache_handle *handle,
		hlcache_handle_callback cb, void *pw)
{
	handle->cb = cb;
	handle->pw = pw;

	return NSERROR_OK;
}

nserror hlcache_handle_clone(hlcache_handle *handle, hlcache_handle **result)
{
	*result = NULL;
	return NSERROR_CLONE_FAILED;
}

/* See hlcache.h for documentation */
nsurl *hlcache_handle_get_url(const hlcache_handle *handle)
{
	nsurl *result = NULL;

	assert(handle != NULL);

	if (handle->entry != NULL) {
		result = content_get_url(handle->entry->content);
	} else {
		RING_ITERATE_START(struct hlcache_retrieval_ctx,
				   hlcache->retrieval_ctx_ring,
				   ictx) {
			if (ictx->handle == handle) {
				/* This is the nascent context for us */
				result = llcache_handle_get_url(ictx->llcache);

				/* And stop */
				RING_ITERATE_STOP(hlcache->retrieval_ctx_ring,
						ictx);
			}
		} RING_ITERATE_END(hlcache->retrieval_ctx_ring, ictx);
	}

	return result;
}
