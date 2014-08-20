#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "content/fetch.h"
#include "content/llcache.h"
#include "utils/ring.h"
#include "utils/nsurl.h"
#include "utils/schedule.h"
#include "utils/url.h"
#include "utils/utils.h"

/******************************************************************************
 * Things that we'd reasonably expect to have to implement                    *
 ******************************************************************************/

/* desktop/netsurf.h */
bool verbose_log;

/* utils/utils.h */
void die(const char * const error)
{
	fprintf(stderr, "%s\n", error);

	exit(1);
}

/* utils/utils.h */
void warn_user(const char *warning, const char *detail)
{
	fprintf(stderr, "%s %s\n", warning, detail);
}

/* utils/utils.h */
char *filename_from_path(char *path)
{
	char *leafname;

	leafname = strrchr(path, '/');
	if (!leafname)
		leafname = path;
	else
		leafname += 1;

	return strdup(leafname);
}

/* utils/schedule.h */
void schedule(int t, schedule_callback_fn cb, void *pw)
{
}

/* utils/schedule.h */
void schedule_remove(schedule_callback_fn cb, void *pw)
{
}

/* content/fetch.h */
const char *fetch_filetype(const char *unix_path)
{
	return NULL;
}

/* content/fetch.h */
char *fetch_mimetype(const char *ro_path)
{
	return NULL;
}

/* utils/url.h */
char *path_to_url(const char *path)
{
	int urllen = strlen(path) + FILE_SCHEME_PREFIX_LEN + 1;
	char *url = malloc(urllen);

	if (url == NULL) {
		return NULL;
	}

	if (*path == '/') {
		path++; /* file: paths are already absolute */
	} 

	snprintf(url, urllen, "%s%s", FILE_SCHEME_PREFIX, path);

	return url;
}

/* utils/url.h */
char *url_to_path(const char *url)
{
	char *url_path = curl_unescape(url, 0);
	char *path;

	/* return the absolute path including leading / */
	path = strdup(url_path + (FILE_SCHEME_PREFIX_LEN - 1));
	curl_free(url_path);

	return path;
}

/******************************************************************************
 * Things that are absolutely not reasonable, and should disappear            *
 ******************************************************************************/

#include "desktop/cookie_manager.h"
#include "desktop/gui.h"
#include "desktop/tree.h"

/* desktop/cookie_manager.h -- used by urldb 
 *
 * URLdb should have a cookies update event + handler registration
 */
bool cookie_manager_add(const struct cookie_data *data)
{
	return true;
}

/* desktop/cookie_manager.h -- used by urldb 
 *
 * URLdb should have a cookies removal handler registration
 */
void cookie_manager_remove(const struct cookie_data *data)
{
}

/* image/bitmap.h -- used by urldb 
 *
 * URLdb shouldn't care about bitmaps. 
 * This is because the legacy RO thumbnail stuff was hacked in and must die.
 */
void bitmap_destroy(void *bitmap)
{
}
/* image/image.h -- used by urldb 
 *
 * URLdb shouldn't care about bitmaps. 
 * This is because the legacy RO thumbnail stuff was hacked in and must die.
 */
bool image_bitmap_plot(struct bitmap *bitmap, struct content_redraw_data *data, 
		const struct rect *clip, const struct redraw_context *ctx)
{
	return true;
}

/* content/fetchers/fetch_file.h -- used by fetcher core
 *
 * Simpler to stub this than haul in all the file fetcher's dependencies
 */
void fetch_file_register(void)
{
}

/* desktop/gui.h -- used by image_cache through about: handler */
nsurl* gui_get_resource_url(const char *path)
{
	return NULL;
}

/******************************************************************************
 * test: protocol handler                                                     *
 ******************************************************************************/

typedef struct test_context {
	struct fetch *parent;

	bool aborted;
	bool locked;

	struct test_context *r_prev;
	struct test_context *r_next;
} test_context;

static test_context *ring;

bool test_initialise(lwc_string *scheme)
{
	/* Nothing to do */
	return true;
}

bool test_can_fetch(const nsurl *url)
{
	/* Nothing to do */
	return true;
}

void test_finalise(lwc_string *scheme)
{
	/* Nothing to do */
}

void *test_setup_fetch(struct fetch *parent, nsurl *url, bool only_2xx,
		bool downgrade_tls, const char *post_urlenc,
		const struct fetch_multipart_data *post_multipart,
		const char **headers)
{
	test_context *ctx = calloc(1, sizeof(test_context));

	if (ctx == NULL)
		return NULL;

	ctx->parent = parent;

	RING_INSERT(ring, ctx);

	return ctx;
}

bool test_start_fetch(void *handle)
{
	/* Nothing to do */
	return true;
}

void test_abort_fetch(void *handle)
{
	test_context *ctx = handle;

	ctx->aborted = true;
}

void test_free_fetch(void *handle)
{
	test_context *ctx = handle;

	RING_REMOVE(ring, ctx);

	free(ctx);
}

void test_process(test_context *ctx)
{
	/** \todo Implement */
}

void test_poll(lwc_string *scheme)
{
	test_context *ctx, *next;

	if (ring == NULL)
		return;

	ctx = ring;
	do {
		next = ctx->r_next;

		if (ctx->locked)
			continue;

		if (ctx->aborted == false) {
			test_process(ctx);
		}

		fetch_remove_from_queues(ctx->parent);
		fetch_free(ctx->parent);
	} while ((ctx = next) != ring && ring != NULL);
}

/******************************************************************************
 * The actual test code                                                       *
 ******************************************************************************/

nserror query_handler(const llcache_query *query, void *pw,
		llcache_query_response cb, void *cbpw)
{
	/* I'm too lazy to actually implement this. It should queue the query, 
	 * then deliver the response from main(). */

	return NSERROR_OK;
}

nserror event_handler(llcache_handle *handle, 
		const llcache_event *event, void *pw)
{
	static char *event_names[] = {
		"HAD_HEADERS", "HAD_DATA", "DONE", "ERROR", "PROGRESS"
	};
	bool *done = pw;

	if (event->type != LLCACHE_EVENT_PROGRESS)
		fprintf(stdout, "%p : %s\n", handle, event_names[event->type]);

	/* Inform main() that the fetch completed */
	if (event->type == LLCACHE_EVENT_DONE)
		*done = true;

	return NSERROR_OK;
}

int main(int argc, char **argv)
{
	nserror error;
	llcache_handle *handle;
	llcache_handle *handle2;
	lwc_string *scheme;
	nsurl *url;
	bool done = false;

	/* Initialise subsystems */
	fetch_init();

	if (lwc_intern_string("test", SLEN("test"), &scheme) != lwc_error_ok) {
		fprintf(stderr, "Failed to intern \"test\"\n");
		return 1;
	}

	fetch_add_fetcher(scheme, test_initialise, test_can_fetch,
			test_setup_fetch, test_start_fetch, test_abort_fetch,
			test_free_fetch, test_poll, test_finalise);

	/* Initialise low-level cache */
	error = llcache_initialise(query_handler, NULL, 1024 * 1024);
	if (error != NSERROR_OK) {
		fprintf(stderr, "llcache_initialise: %d\n", error);
		return 1;
	}

	if (nsurl_create("http://www.netsurf-browser.org", &url) != NSERROR_OK) {
		fprintf(stderr, "Failed creating url\n");
		return 1;
	}

	/* Retrieve an URL from the low-level cache (may trigger fetch) */
	error = llcache_handle_retrieve(url, 
			LLCACHE_RETRIEVE_VERIFIABLE, NULL, NULL,
			event_handler, &done, &handle);
	if (error != NSERROR_OK) {
		fprintf(stderr, "llcache_handle_retrieve: %d\n", error);
		return 1;
	}

	/* Poll relevant components */
	while (done == false) {
		llcache_poll();
	}

	done = false;
	error = llcache_handle_retrieve(url,
			LLCACHE_RETRIEVE_VERIFIABLE, NULL, NULL,
			event_handler, &done, &handle2);
	if (error != NSERROR_OK) {
		fprintf(stderr, "llcache_handle_retrieve: %d\n", error);
		return 1;
	}

	while (done == false) {
		llcache_poll();
	}

	fprintf(stdout, "%p, %p -> %d\n", handle, handle2,
			llcache_handle_references_same_object(handle, handle2));

	/* Cleanup */
	llcache_handle_release(handle2);
	llcache_handle_release(handle);

	fetch_quit();

	return 0;
}

