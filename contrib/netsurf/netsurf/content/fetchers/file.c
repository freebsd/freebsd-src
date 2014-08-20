/*
 * Copyright 2010 Vincent Sanders <vince@netsurf-browser.org>
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

/* file: URL handling. Based on the data fetcher by Rob Kendrick */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <stdarg.h>

#include "utils/config.h"

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

#include <libwapcaplet/libwapcaplet.h>

#include "content/dirlist.h"
#include "content/fetch.h"
#include "content/fetchers/file.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "desktop/gui_factory.h"
#include "utils/corestrings.h"
#include "utils/nsoption.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/ring.h"

/* Maximum size of read buffer */
#define FETCH_FILE_MAX_BUF_SIZE (1024 * 1024)

/** Context for a fetch */
struct fetch_file_context {
	struct fetch_file_context *r_next, *r_prev;

	struct fetch *fetchh; /**< Handle for this fetch */

	bool aborted; /**< Flag indicating fetch has been aborted */
	bool locked; /**< Flag indicating entry is already entered */

	nsurl *url; /**< The full url the fetch refers to */
	char *path; /**< The actual path to be used with open() */

	time_t file_etag; /**< Request etag for file (previous st.m_time) */
};

static struct fetch_file_context *ring = NULL;

/** issue fetch callbacks with locking */
static inline bool fetch_file_send_callback(const fetch_msg *msg,
		struct fetch_file_context *ctx)
{
	ctx->locked = true;
	fetch_send_callback(msg, ctx->fetchh);
	ctx->locked = false;

	return ctx->aborted;
}

static bool fetch_file_send_header(struct fetch_file_context *ctx,
		const char *fmt, ...)
{
	fetch_msg msg;
	char header[64];
	va_list ap;

	va_start(ap, fmt);

	vsnprintf(header, sizeof header, fmt, ap);

	va_end(ap);

	msg.type = FETCH_HEADER;
	msg.data.header_or_data.buf = (const uint8_t *) header;
	msg.data.header_or_data.len = strlen(header);
	fetch_file_send_callback(&msg, ctx);

	return ctx->aborted;
}

/** callback to initialise the file fetcher. */
static bool fetch_file_initialise(lwc_string *scheme)
{
	return true;
}

/** callback to initialise the file fetcher. */
static void fetch_file_finalise(lwc_string *scheme)
{
}

static bool fetch_file_can_fetch(const nsurl *url)
{
	return true;
}

/** callback to set up a file fetch context. */
static void *
fetch_file_setup(struct fetch *fetchh,
		 nsurl *url,
		 bool only_2xx,
		 bool downgrade_tls,
		 const char *post_urlenc,
		 const struct fetch_multipart_data *post_multipart,
		 const char **headers)
{
	struct fetch_file_context *ctx;
	int i;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		return NULL;

	ctx->path = guit->fetch->url_to_path(nsurl_access(url));
	if (ctx->path == NULL) {
		free(ctx);
		return NULL;
	}

	ctx->url = nsurl_ref(url);

	/* Scan request headers looking for If-None-Match */
	for (i = 0; headers[i] != NULL; i++) {
		if (strncasecmp(headers[i], "If-None-Match:", 
				SLEN("If-None-Match:")) == 0) {
			/* If-None-Match: "12345678" */
			const char *d = headers[i] + SLEN("If-None-Match:");

			/* Scan to first digit, if any */
			while (*d != '\0' && (*d < '0' || '9' < *d))
				d++;

			/* Convert to time_t */
			if (*d != '\0')
				ctx->file_etag = atoi(d);
		}
	}

	ctx->fetchh = fetchh;

	RING_INSERT(ring, ctx);

	return ctx;
}

/** callback to free a file fetch */
static void fetch_file_free(void *ctx)
{
	struct fetch_file_context *c = ctx;
	nsurl_unref(c->url);
	free(c->path);
	RING_REMOVE(ring, c);
	free(ctx);
}

/** callback to start a file fetch */
static bool fetch_file_start(void *ctx)
{
	return true;
}

/** callback to abort a file fetch */
static void fetch_file_abort(void *ctx)
{
	struct fetch_file_context *c = ctx;

	/* To avoid the poll loop having to deal with the fetch context
	 * disappearing from under it, we simply flag the abort here.
	 * The poll loop itself will perform the appropriate cleanup.
	 */
	c->aborted = true;
}

static int fetch_file_errno_to_http_code(int error_no)
{
	switch (error_no) {
	case ENAMETOOLONG:
		return 400;
	case EACCES:
		return 403;
	case ENOENT:
		return 404;
	default:
		break;
	}

	return 500;
}

static void fetch_file_process_error(struct fetch_file_context *ctx, int code)
{
	fetch_msg msg;
	char buffer[1024];
	const char *title;
	char key[8];

	/* content is going to return error code */
	fetch_set_http_code(ctx->fetchh, code);

	/* content type */
	if (fetch_file_send_header(ctx, "Content-Type: text/html"))
		goto fetch_file_process_error_aborted;

	snprintf(key, sizeof key, "HTTP%03d", code);
	title = messages_get(key);

	snprintf(buffer, sizeof buffer, "<html><head><title>%s</title></head>"
			"<body><h1>%s</h1>"
			"<p>Error %d while fetching file %s</p></body></html>",
			title, title, code, nsurl_access(ctx->url));

	msg.type = FETCH_DATA;
	msg.data.header_or_data.buf = (const uint8_t *) buffer;
	msg.data.header_or_data.len = strlen(buffer);
	if (fetch_file_send_callback(&msg, ctx))
		goto fetch_file_process_error_aborted;

	msg.type = FETCH_FINISHED;
	fetch_file_send_callback(&msg, ctx);

fetch_file_process_error_aborted:
	return;
}


/** Process object as a regular file */
static void fetch_file_process_plain(struct fetch_file_context *ctx,
				     struct stat *fdstat)
{
#ifdef HAVE_MMAP
	fetch_msg msg;
	char *buf = NULL;
	size_t buf_size;

	int fd; /**< The file descriptor of the object */

	/* Check if we can just return not modified */
	if (ctx->file_etag != 0 && ctx->file_etag == fdstat->st_mtime) {
		fetch_set_http_code(ctx->fetchh, 304);
		msg.type = FETCH_NOTMODIFIED;
		fetch_file_send_callback(&msg, ctx);
		return;
	}

	fd = open(ctx->path, O_RDONLY);
	if (fd < 0) {
		/* process errors as appropriate */
		fetch_file_process_error(ctx,
				fetch_file_errno_to_http_code(errno));
		return;
	}

	/* set buffer size */
	buf_size = fdstat->st_size;

	/* allocate the buffer storage */
	if (buf_size > 0) {
		buf = mmap(NULL, buf_size, PROT_READ, MAP_SHARED, fd, 0);
		if (buf == MAP_FAILED) {
			msg.type = FETCH_ERROR;
			msg.data.error = "Unable to map memory for file data buffer";
			fetch_file_send_callback(&msg, ctx);
			close(fd);
			return;
		}
	}

	/* fetch is going to be successful */
	fetch_set_http_code(ctx->fetchh, 200);

	/* Any callback can result in the fetch being aborted.
	 * Therefore, we _must_ check for this after _every_ call to
	 * fetch_file_send_callback().
	 */

	/* content type */
	if (fetch_file_send_header(ctx, "Content-Type: %s", 
			guit->fetch->filetype(ctx->path)))
		goto fetch_file_process_aborted;

	/* content length */
	if (fetch_file_send_header(ctx, "Content-Length: %"SSIZET_FMT, fdstat->st_size))
		goto fetch_file_process_aborted;

	/* create etag */
	if (fetch_file_send_header(ctx, "ETag: \"%10" PRId64 "\"",
			(int64_t) fdstat->st_mtime))
		goto fetch_file_process_aborted;


	msg.type = FETCH_DATA;
	msg.data.header_or_data.buf = (const uint8_t *) buf;
	msg.data.header_or_data.len = buf_size;
	fetch_file_send_callback(&msg, ctx);

	if (ctx->aborted == false) {
		msg.type = FETCH_FINISHED;
		fetch_file_send_callback(&msg, ctx);
	}

fetch_file_process_aborted:

	if (buf != NULL)
		munmap(buf, buf_size);
	close(fd);
#else
	fetch_msg msg;
	char *buf;
	size_t buf_size;

	ssize_t tot_read = 0;
	ssize_t res;

	FILE *infile;

	/* Check if we can just return not modified */
	if (ctx->file_etag != 0 && ctx->file_etag == fdstat->st_mtime) {
		fetch_set_http_code(ctx->fetchh, 304);
		msg.type = FETCH_NOTMODIFIED;
		fetch_file_send_callback(&msg, ctx);
		return;
	}

	infile = fopen(ctx->path, "rb");
	if (infile == NULL) {
		/* process errors as appropriate */
		fetch_file_process_error(ctx,
				fetch_file_errno_to_http_code(errno));
		return;
	}

	/* set buffer size */
	buf_size = fdstat->st_size;
	if (buf_size > FETCH_FILE_MAX_BUF_SIZE)
		buf_size = FETCH_FILE_MAX_BUF_SIZE;

	/* allocate the buffer storage */
	buf = malloc(buf_size);
	if (buf == NULL) {
		msg.type = FETCH_ERROR;
		msg.data.error =
			"Unable to allocate memory for file data buffer";
		fetch_file_send_callback(&msg, ctx);
		fclose(infile);
		return;
	}

	/* fetch is going to be successful */
	fetch_set_http_code(ctx->fetchh, 200);

	/* Any callback can result in the fetch being aborted.
	 * Therefore, we _must_ check for this after _every_ call to
	 * fetch_file_send_callback().
	 */

	/* content type */
	if (fetch_file_send_header(ctx, "Content-Type: %s", 
			guit->fetch->filetype(ctx->path)))
		goto fetch_file_process_aborted;

	/* content length */
	if (fetch_file_send_header(ctx, "Content-Length: %"SSIZET_FMT, fdstat->st_size))
		goto fetch_file_process_aborted;

	/* create etag */
	if (fetch_file_send_header(ctx, "ETag: \"%10" PRId64 "\"", 
			(int64_t) fdstat->st_mtime))
		goto fetch_file_process_aborted;

	/* main data loop */
	while (tot_read < fdstat->st_size) {
		res = fread(buf, 1, buf_size, infile);
		if (res == 0) {
			if (feof(infile)) {
				msg.type = FETCH_ERROR;
				msg.data.error = "Unexpected EOF reading file";
				fetch_file_send_callback(&msg, ctx);
				goto fetch_file_process_aborted;
			} else {
				msg.type = FETCH_ERROR;
				msg.data.error = "Error reading file";
				fetch_file_send_callback(&msg, ctx);
				goto fetch_file_process_aborted;
			}
		}
		tot_read += res;

		msg.type = FETCH_DATA;
		msg.data.header_or_data.buf = (const uint8_t *) buf;
		msg.data.header_or_data.len = res;
		if (fetch_file_send_callback(&msg, ctx))
			break;
	}

	if (ctx->aborted == false) {
		msg.type = FETCH_FINISHED;
		fetch_file_send_callback(&msg, ctx);
	}

fetch_file_process_aborted:

	fclose(infile);
	free(buf);
#endif
	return;
}

static char *gen_nice_title(char *path)
{
	char *nice_path, *cnv, *tmp;
	char *title;
	int title_length;

	/* Convert path for display */
	nice_path = malloc(strlen(path) * SLEN("&amp;") + 1);
	if (nice_path == NULL) {
		return NULL;
	}

	/* Escape special HTML characters */
	for (cnv = nice_path, tmp = path; *tmp != '\0'; tmp++) {
		if (*tmp == '<') {
			*cnv++ = '&';
			*cnv++ = 'l';
			*cnv++ = 't';
			*cnv++ = ';';
		} else if (*tmp == '>') {
			*cnv++ = '&';
			*cnv++ = 'g';
			*cnv++ = 't';
			*cnv++ = ';';
		} else if (*tmp == '&') {
			*cnv++ = '&';
			*cnv++ = 'a';
			*cnv++ = 'm';
			*cnv++ = 'p';
			*cnv++ = ';';
		} else {
			*cnv++ = *tmp;
		}
	}
	*cnv = '\0';

	/* Construct a localised title string */
	title_length = (cnv - nice_path) + strlen(messages_get("FileIndex"));
	title = malloc(title_length + 1);

	if (title == NULL) {
		free(nice_path);
		return NULL;
	}

	/* Set title to localised "Index of <nice_path>" */
	snprintf(title, title_length, messages_get("FileIndex"), nice_path);

	free(nice_path);

	return title;
}


static void fetch_file_process_dir(struct fetch_file_context *ctx,
				   struct stat *fdstat)
{
	fetch_msg msg;
	char buffer[1024]; /* Output buffer */
	bool even = false; /* formatting flag */
	char *title; /* pretty printed title */
	nserror err; /* result from url routines */
	nsurl *up; /* url of parent */
	char *path; /* url for list entries */

	struct stat ent_stat; /* stat result of leaf entry */
	char datebuf[64]; /* buffer for date text */
	char timebuf[64]; /* buffer for time text */
	char urlpath[PATH_MAX]; /* buffer for leaf entry path */
	struct dirent *ent; /* current directory entry */
	struct dirent **listing = NULL; /* directory entry listing */
	int i; /* directory entry index */
	int n; /* number of directory entries */

	n = scandir(ctx->path, &listing, 0, dir_sort_alpha);
	if (n < 0) {
		fetch_file_process_error(ctx,
			fetch_file_errno_to_http_code(errno));
		return;
	}

	/* fetch is going to be successful */
	fetch_set_http_code(ctx->fetchh, 200);

	/* force no-cache */
	if (fetch_file_send_header(ctx, "Cache-Control: no-cache"))
		goto fetch_file_process_dir_aborted;

	/* content type */
	if (fetch_file_send_header(ctx, "Content-Type: text/html"))
		goto fetch_file_process_dir_aborted;

	msg.type = FETCH_DATA;
	msg.data.header_or_data.buf = (const uint8_t *) buffer;

	/* directory listing top */
	dirlist_generate_top(buffer, sizeof buffer);
	msg.data.header_or_data.len = strlen(buffer);
	if (fetch_file_send_callback(&msg, ctx))
		goto fetch_file_process_dir_aborted;

	/* directory listing title */
	title = gen_nice_title(ctx->path);
	dirlist_generate_title(title, buffer, sizeof buffer);
	free(title);
	msg.data.header_or_data.len = strlen(buffer);
	if (fetch_file_send_callback(&msg, ctx))
		goto fetch_file_process_dir_aborted;

	/* Print parent directory link */
	err = nsurl_parent(ctx->url, &up);
	if (err == NSERROR_OK) {
		if (nsurl_compare(ctx->url, up, NSURL_COMPLETE) == false) {
			/* different URL; have parent */
			dirlist_generate_parent_link(nsurl_access(up),
					buffer, sizeof buffer);

			msg.data.header_or_data.len = strlen(buffer);
			fetch_file_send_callback(&msg, ctx);
		}
		nsurl_unref(up);

		if (ctx->aborted)
			goto fetch_file_process_dir_aborted;

	}

	/* directory list headings */
	dirlist_generate_headings(buffer, sizeof buffer);
	msg.data.header_or_data.len = strlen(buffer);
	if (fetch_file_send_callback(&msg, ctx))
		goto fetch_file_process_dir_aborted;

	for (i = 0; i < n; i++) {
		ent = listing[i];

		if (ent->d_name[0] == '.')
			continue;

		strncpy(urlpath, ctx->path, sizeof urlpath);
		if (guit->fetch->path_add_part(urlpath, sizeof urlpath,
				ent->d_name) == false)
			continue;

		if (stat(urlpath, &ent_stat) != 0) {
			ent_stat.st_mode = 0;
			datebuf[0] = 0;
			timebuf[0] = 0;
		} else {
			/* Get date in output format */
			if (strftime((char *)&datebuf, sizeof datebuf,
				     "%a %d %b %Y",
				     localtime(&ent_stat.st_mtime)) == 0) {
				strncpy(datebuf, "-", sizeof datebuf);
			}

			/* Get time in output format */
			if (strftime((char *)&timebuf, sizeof timebuf,
				     "%H:%M",
				     localtime(&ent_stat.st_mtime)) == 0) {
				strncpy(timebuf, "-", sizeof timebuf);
			}
		}

		if((path = guit->fetch->path_to_url(urlpath)) == NULL)
			continue;

		if (S_ISREG(ent_stat.st_mode)) {
			/* regular file */
			dirlist_generate_row(even,
					     false,
					     path,
					     ent->d_name,
					     guit->fetch->filetype(urlpath),
					     ent_stat.st_size,
					     datebuf, timebuf,
					     buffer, sizeof(buffer));
		} else if (S_ISDIR(ent_stat.st_mode)) {
			/* directory */
			dirlist_generate_row(even,
					     true,
					     path,
					     ent->d_name,
					     messages_get("FileDirectory"),
					     -1,
					     datebuf, timebuf,
					     buffer, sizeof(buffer));
		} else {
			/* something else */
			dirlist_generate_row(even,
					     false,
					     path,
					     ent->d_name,
					     "",
					     -1,
					     datebuf, timebuf,
					     buffer, sizeof(buffer));
		}

		free(path);

		msg.data.header_or_data.len = strlen(buffer);
		if (fetch_file_send_callback(&msg, ctx))
			goto fetch_file_process_dir_aborted;

		even = !even;
	}

	/* directory listing bottom */
	dirlist_generate_bottom(buffer, sizeof buffer);
	msg.data.header_or_data.len = strlen(buffer);
	if (fetch_file_send_callback(&msg, ctx))
		goto fetch_file_process_dir_aborted;

	msg.type = FETCH_FINISHED;
	fetch_file_send_callback(&msg, ctx);

fetch_file_process_dir_aborted:

	if (listing != NULL) {
		for (i = 0; i < n; i++) {
			free(listing[i]);
		}
		free(listing);
	}
}


/* process a file fetch */
static void fetch_file_process(struct fetch_file_context *ctx)
{
	struct stat fdstat; /**< The objects stat */

	if (stat(ctx->path, &fdstat) != 0) {
		/* process errors as appropriate */
		fetch_file_process_error(ctx,
				fetch_file_errno_to_http_code(errno));
		return;
	}

	if (S_ISDIR(fdstat.st_mode)) {
		/* directory listing */
		fetch_file_process_dir(ctx, &fdstat);
		return;
	} else if (S_ISREG(fdstat.st_mode)) {
		/* regular file */
		fetch_file_process_plain(ctx, &fdstat);
		return;
	} else {
		/* unhandled type of file */
		fetch_file_process_error(ctx, 501);
	}

	return;
}

/** callback to poll for additional file fetch contents */
static void fetch_file_poll(lwc_string *scheme)
{
	struct fetch_file_context *c, *next;

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
		if (c->aborted == false) {
			/* file fetches can be processed in one go */
			fetch_file_process(c);
		}

		/* Compute next fetch item at the last possible moment as
		 * processing this item may have added to the ring.
		 */
		next = c->r_next;

		fetch_remove_from_queues(c->fetchh);
		fetch_free(c->fetchh);

		/* Advance to next ring entry, exiting if we've reached
		 * the start of the ring or the ring has become empty
		 */
	} while ( (c = next) != ring && ring != NULL);
}

void fetch_file_register(void)
{
	lwc_string *scheme = lwc_string_ref(corestring_lwc_file);

	fetch_add_fetcher(scheme,
		fetch_file_initialise,
		fetch_file_can_fetch,
		fetch_file_setup,
		fetch_file_start,
		fetch_file_abort,
		fetch_file_free,
		fetch_file_poll,
		fetch_file_finalise);
}
