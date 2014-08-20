/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Rob Jackson <jacko@xms.ms>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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
 * Download windows (RISC OS implementation).
 *
 * This file implements the interface given by desktop/gui.h for download
 * windows. Each download window has an associated fetch. Downloads start by
 * writing received data to a temporary file. At some point the user chooses
 * a destination (by drag & drop), and the temporary file is then moved to the
 * destination and the download continues until complete.
 */

#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <curl/curl.h>
#include "oslib/mimemap.h"
#include "oslib/osargs.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osfscontrol.h"
#include "oslib/osgbpb.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"

#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/download.h"
#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"

#include "riscos/gui.h"
#include "riscos/dialog.h"
#include "riscos/mouse.h"
#include "riscos/save.h"
#include "riscos/query.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/ucstables.h"

#define ICON_DOWNLOAD_ICON 0
#define ICON_DOWNLOAD_URL 1
#define ICON_DOWNLOAD_PATH 2
#define ICON_DOWNLOAD_DESTINATION 3
#define ICON_DOWNLOAD_PROGRESS 5
#define ICON_DOWNLOAD_STATUS 6

#define RO_DOWNLOAD_MAX_PATH_LEN 255

typedef enum
{
	QueryRsn_Quit,
	QueryRsn_Abort,
	QueryRsn_Overwrite
} query_reason;


/** Data for a download window. */
struct gui_download_window {
	/** Associated context, or 0 if the fetch has completed or aborted. */
	download_context *ctx;
	unsigned int received;	/**< Amount of data received so far. */
	unsigned int total_size; /**< Size of resource, or 0 if unknown. */

	wimp_w window;		/**< RISC OS window handle. */
	bits file_type;		/**< RISC OS file type. */

	char url[256];		/**< Buffer for URL icon. */
	char sprite_name[20];	/**< Buffer for sprite icon. */
	char path[RO_DOWNLOAD_MAX_PATH_LEN];	/**< Buffer for pathname icon. */
	char status[256];	/**< Buffer for status icon. */

	/** User has chosen the destination, and it is being written. */
	bool saved;
	bool close_confirmed;
	bool error;		/**< Error occurred, aborted. */

	/** RISC OS file handle, of temporary file when !saved, and of
	 * destination when saved. */
	os_fw file;

	query_id query;
	query_reason query_rsn;

	struct timeval start_time;	/**< Time download started. */
	struct timeval last_time;	/**< Time status was last updated. */
	unsigned int last_received;	/**< Value of received at last_time. */
	float average_rate;		/**< Moving average download rate. */
	unsigned int average_points;	/**< Number of points in the average. */

	bool send_dataload;	/**< Should send DataLoad message when finished */
	wimp_message save_message;	/**< Copy of wimp DataSaveAck message */

	struct gui_download_window *prev;	/**< Previous in linked list. */
	struct gui_download_window *next;	/**< Next in linked list. */
};


/** List of all download windows. */
static struct gui_download_window *download_window_list = 0;
/** Download window with current save operation. */
static struct gui_download_window *download_window_current = 0;

/** Template for a download window. */
static wimp_window *download_template;

/** Width of progress bar at 100%. */
static int download_progress_width;
/** Coordinates of progress bar. */
static int download_progress_x0;
static int download_progress_y0;
static int download_progress_y1;

/** Current download directory. */
static char *download_dir = NULL;
static size_t download_dir_len;

static void ro_gui_download_drag_end(wimp_dragged *drag, void *data);
static const char *ro_gui_download_temp_name(struct gui_download_window *dw);
static void ro_gui_download_update_status(struct gui_download_window *dw);
static void ro_gui_download_update_status_wrapper(void *p);
static void ro_gui_download_window_hide_caret(struct gui_download_window *dw);
static char *ro_gui_download_canonicalise(const char *path);
static bool ro_gui_download_check_space(struct gui_download_window *dw,
		const char *dest_file, const char *orig_file);
static os_error *ro_gui_download_move(struct gui_download_window *dw,
		const char *dest_file, const char *src_file);
static void ro_gui_download_remember_dir(const char *path);
static bool ro_gui_download_save(struct gui_download_window *dw,
		const char *file_name, bool force_overwrite);
static void ro_gui_download_send_dataload(struct gui_download_window *dw);
static void ro_gui_download_window_destroy_wrapper(void *p);
static bool ro_gui_download_window_destroy(struct gui_download_window *dw, bool quit);
static void ro_gui_download_close_confirmed(query_id, enum query_response res, void *p);
static void ro_gui_download_close_cancelled(query_id, enum query_response res, void *p);
static void ro_gui_download_overwrite_confirmed(query_id, enum query_response res, void *p);
static void ro_gui_download_overwrite_cancelled(query_id, enum query_response res, void *p);

static bool ro_gui_download_click(wimp_pointer *pointer);
static bool ro_gui_download_keypress(wimp_key *key);
static void ro_gui_download_close(wimp_w w);

static const query_callback close_funcs =
{
	ro_gui_download_close_confirmed,
	ro_gui_download_close_cancelled
};

static const query_callback overwrite_funcs =
{
	ro_gui_download_overwrite_confirmed,
	ro_gui_download_overwrite_cancelled
};


/**
 * Load the download window template.
 */

void ro_gui_download_init(void)
{
	download_template = ro_gui_dialog_load_template("download");
	download_progress_width =
		download_template->icons[ICON_DOWNLOAD_STATUS].extent.x1 -
		download_template->icons[ICON_DOWNLOAD_STATUS].extent.x0;
	download_progress_x0 =
		download_template->icons[ICON_DOWNLOAD_PROGRESS].extent.x0;
	download_progress_y0 =
		download_template->icons[ICON_DOWNLOAD_PROGRESS].extent.y0;
	download_progress_y1 =
		download_template->icons[ICON_DOWNLOAD_PROGRESS].extent.y1;
}


/**
 * Returns the pathname of a temporary file for this download.
 *
 * \param  dw   download window
 * \return ptr to pathname
 */

const char *ro_gui_download_temp_name(struct gui_download_window *dw)
{
	static char temp_name[40];
	snprintf(temp_name, sizeof temp_name, "<Wimp$ScrapDir>.ns%x",
			(unsigned int) dw);
	return temp_name;
}


/**
 * Create and open a download progress window.
 *
 * \param  ctx         Download context
 * \return  a new gui_download_window structure, or 0 on error and error
 *          reported
 */

static struct gui_download_window *gui_download_window_create(download_context *ctx,
		struct gui_window *gui)
{
	const char *url = download_context_get_url(ctx);
	const char *mime_type = download_context_get_mime_type(ctx);
	const char *temp_name;
	char *scheme = NULL;
	char *filename = NULL;
	struct gui_download_window *dw;
	bool space_warning = false;
	os_error *error;
	url_func_result res;
	char *local_path;
	nserror err;
	size_t i, last_dot;

	dw = malloc(sizeof *dw);
	if (!dw) {
		warn_user("NoMemory", 0);
		return 0;
	}

	dw->ctx = ctx;
	dw->saved = false;
	dw->close_confirmed = false;
	dw->error = false;
	dw->query = QUERY_INVALID;
	dw->received = 0;
	dw->total_size = download_context_get_total_length(ctx);
	strncpy(dw->url, url, sizeof dw->url);
	dw->url[sizeof dw->url - 1] = 0;
	dw->status[0] = 0;
	gettimeofday(&dw->start_time, 0);
	dw->last_time = dw->start_time;
	dw->last_received = 0;
	dw->file_type = 0;
	dw->average_rate = 0;
	dw->average_points = 0;

	/* Get scheme from URL */
	res = url_scheme(url, &scheme);
	if (res == URL_FUNC_NOMEM) {
		warn_user("NoMemory", 0);
		free(dw);
		return 0;
	} else if (res == URL_FUNC_OK) {
		/* If we have a scheme and it's "file", then
		 * attempt to use the local filetype directly */
		if (strcasecmp(scheme, "file") == 0) {
			char *path = NULL;
			res = url_path(url, &path);
			if (res == URL_FUNC_NOMEM) {
				warn_user("NoMemory", 0);
				free(scheme);
				free(dw);
				return 0;
			} else if (res == URL_FUNC_OK) {
				char *raw_path = curl_unescape(path,
						strlen(path));
				if (raw_path == NULL) {
					warn_user("NoMemory", 0);
					free(path);
					free(scheme);
					free(dw);
					return 0;
				}
				dw->file_type =
					ro_filetype_from_unix_path(raw_path);
				curl_free(raw_path);
				free(path);
			}
		}

		free(scheme);
	}

	/* If we still don't have a filetype (i.e. failed reading local
	 * one or fetching a remote object), then use the MIME type */
	if (dw->file_type == 0) {
		/* convert MIME type to RISC OS file type */
		error = xmimemaptranslate_mime_type_to_filetype(mime_type,
				&(dw->file_type));
		if (error) {
			LOG(("xmimemaptranslate_mime_type_to_filetype: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
			dw->file_type = 0xffd;
		}
	}

	/* open temporary output file */
	temp_name = ro_gui_download_temp_name(dw);
	if (!ro_gui_download_check_space(dw, temp_name, NULL)) {
		/* issue a warning but continue with the download because the
		   user can save it to another medium whilst it's downloading */
		space_warning = true;
	}
	error = xosfind_openoutw(osfind_NO_PATH | osfind_ERROR_IF_DIR,
			temp_name, 0, &dw->file);
	if (error) {
		LOG(("xosfind_openoutw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		free(dw);
		return 0;
	}

	/* fill in download window icons */
	download_template->icons[ICON_DOWNLOAD_URL].data.indirected_text.text =
			dw->url;
	download_template->icons[ICON_DOWNLOAD_URL].data.indirected_text.size =
			sizeof dw->url;

	download_template->icons[ICON_DOWNLOAD_STATUS].data.indirected_text.
			text = dw->status;
	download_template->icons[ICON_DOWNLOAD_STATUS].data.indirected_text.
			size = sizeof dw->status;

	sprintf(dw->sprite_name, "file_%.3x", dw->file_type);
	if (!ro_gui_wimp_sprite_exists(dw->sprite_name))
		strcpy(dw->sprite_name, "file_xxx");
	download_template->icons[ICON_DOWNLOAD_ICON].data.indirected_sprite.id =
			(osspriteop_id) dw->sprite_name;

	/* Get a suitable path- and leafname for the download. */
	temp_name = download_context_get_filename(dw->ctx);

	if (temp_name == NULL)
		temp_name = messages_get("SaveObject");

	if (temp_name != NULL)
		filename = strdup(temp_name);

	if (filename == NULL) {
		LOG(("Failed to establish download filename."));
		warn_user("SaveError", error->errmess);
		free(dw);
		return 0;
	}

	for (i = 0, last_dot = (size_t) -1; filename[i] != '\0'; i++) {
		const char c = filename[i];

		if (c == '.') {
			last_dot = i;
			filename[i] = '/';
		} else if (c <= ' ' || strchr(":*#$&@^%\\", c) != NULL)
			filename[i] = '_';
	}

	if (nsoption_bool(strip_extensions) && last_dot != (size_t) -1)
		filename[last_dot] = '\0';

	if (download_dir != NULL && strlen(download_dir) > 0)
		snprintf(dw->path, RO_DOWNLOAD_MAX_PATH_LEN, "%s.%s",
				download_dir, filename);
	else
		snprintf(dw->path, RO_DOWNLOAD_MAX_PATH_LEN, "%s",
				filename);

	free(filename);

	err = utf8_to_local_encoding(dw->path, 0, &local_path);
	if (err != NSERROR_OK) {
		/* badenc should never happen */
		assert(err !=NSERROR_BAD_ENCODING);
		LOG(("utf8_to_local_encoding failed"));
		warn_user("NoMemory", 0);
		free(dw);
		return 0;
	}
	else {
		strncpy(dw->path, local_path, sizeof dw->path);
		free(local_path);
	}

	download_template->icons[ICON_DOWNLOAD_PATH].data.indirected_text.text =
			dw->path;
	download_template->icons[ICON_DOWNLOAD_PATH].data.indirected_text.size =
			sizeof dw->path;

	download_template->icons[ICON_DOWNLOAD_DESTINATION].data.
			indirected_text.text = dw->path;
	download_template->icons[ICON_DOWNLOAD_DESTINATION].data.
			indirected_text.size = sizeof dw->path;

	download_template->icons[ICON_DOWNLOAD_DESTINATION].flags |=
			wimp_ICON_DELETED;

	/* create and open the download window */
	error = xwimp_create_window(download_template, &dw->window);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		free(dw);
		return 0;
	}

	dw->prev = 0;
	dw->next = download_window_list;
	if (download_window_list)
		download_window_list->prev = dw;
	download_window_list = dw;

	ro_gui_download_update_status(dw);

	ro_gui_dialog_open(dw->window);

	ro_gui_wimp_event_set_user_data(dw->window, dw);
	ro_gui_wimp_event_register_mouse_click(dw->window, ro_gui_download_click);
	ro_gui_wimp_event_register_keypress(dw->window, ro_gui_download_keypress);
	ro_gui_wimp_event_register_close_window(dw->window, ro_gui_download_close);

	/* issue the warning now, so that it appears in front of the download
	 * window! */
	if (space_warning)
		warn_user("DownloadWarn", messages_get("NoDiscSpace"));

	return dw;
}

/**
 * Handle failed downloads.
 *
 * \param  dw         download window
 * \param  error_msg  error message
 */

static void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg)
{
	os_error *error;

	if (dw->ctx != NULL)
		download_context_destroy(dw->ctx);
	dw->ctx = NULL;
	dw->error = true;

	riscos_schedule(-1, ro_gui_download_update_status_wrapper, dw);

	/* place error message in status icon in red */
	strncpy(dw->status, error_msg, sizeof dw->status);
	error = xwimp_set_icon_state(dw->window,
			ICON_DOWNLOAD_STATUS,
			wimp_COLOUR_RED << wimp_ICON_FG_COLOUR_SHIFT,
			wimp_ICON_FG_COLOUR);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	/* grey out pathname icon */
	error = xwimp_set_icon_state(dw->window, ICON_DOWNLOAD_PATH,
			wimp_ICON_SHADED, 0);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	/* grey out file icon */
	error = xwimp_set_icon_state(dw->window, ICON_DOWNLOAD_ICON,
			wimp_ICON_SHADED, wimp_ICON_SHADED);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	ro_gui_download_window_hide_caret(dw);
}

/**
 * Handle received download data.
 *
 * \param  dw    download window
 * \param  data  pointer to block of data received
 * \param  size  size of data
 * \return NSERROR_OK on success, appropriate error otherwise
 */

static nserror gui_download_window_data(struct gui_download_window *dw,
		const char *data, unsigned int size)
{
	while (true) {
		const char *msg;
		int unwritten;
		os_error *error;

		error = xosgbpb_writew(dw->file, (const byte *) data, size,
				&unwritten);
		if (error) {
			LOG(("xosgbpb_writew: 0x%x: %s",
					error->errnum, error->errmess));
			msg = error->errmess;

		} else if (unwritten) {
			LOG(("xosgbpb_writew: unwritten %i", unwritten));
			msg = messages_get("Unwritten");
		}
		else {
			dw->received += size;
			return NSERROR_OK;
		}

		warn_user("SaveError", msg);

		if (dw->saved) {
			/* try to continue with the temporary file */
			const char *temp_name = ro_gui_download_temp_name(dw);

			error = ro_gui_download_move(dw, temp_name, dw->path);
			if (!error) {

				/* re-allow saving */
				dw->saved = false;

				error = xwimp_set_icon_state(dw->window, ICON_DOWNLOAD_ICON,
						wimp_ICON_SHADED, 0);
				if (error) {
					LOG(("xwimp_set_icon_state: 0x%x: %s",
							error->errnum, error->errmess));
					warn_user("WimpError", error->errmess);
				}

				error = xwimp_set_icon_state(dw->window, ICON_DOWNLOAD_DESTINATION,
						wimp_ICON_DELETED, wimp_ICON_DELETED);
				if (error) {
					LOG(("xwimp_set_icon_state: 0x%x: %s",
							error->errnum, error->errmess));
					warn_user("WimpError", error->errmess);
				}
				error = xwimp_set_icon_state(dw->window,
						ICON_DOWNLOAD_PATH, wimp_ICON_DELETED, 0);
				if (error) {
					LOG(("xwimp_set_icon_state: 0x%x: %s",
							error->errnum, error->errmess));
					warn_user("WimpError", error->errmess);
				}

				continue;
			}
		}

		/* give up then */
		assert(dw->ctx);
		download_context_abort(dw->ctx);
		gui_download_window_error(dw, msg);

		return NSERROR_SAVE_FAILED;
	}
}


/**
 * Update the status text and progress bar.
 *
 * \param  dw  download window
 */

void ro_gui_download_update_status(struct gui_download_window *dw)
{
	char *received;
	char *total_size;
	char *speed;
	char time[20] = "?";
	struct timeval t;
	float dt;
	unsigned int left;
	float rate;
	os_error *error;
	int width;
	char *local_status;
	nserror err;

	gettimeofday(&t, 0);
	dt = (t.tv_sec + 0.000001 * t.tv_usec) - (dw->last_time.tv_sec +
			0.000001 * dw->last_time.tv_usec);
	if (dt == 0)
		dt = 0.001;

	total_size = human_friendly_bytesize(max(dw->received, dw->total_size));

	if (dw->ctx) {
		rate = (dw->received - dw->last_received) / dt;
		received = human_friendly_bytesize(dw->received);
		/* A simple 'modified moving average' download rate calculation
		 * to smooth out rate fluctuations: chosen for simplicity.
		 */
		dw->average_points++;
		dw->average_rate =
				((dw->average_points - 1) *
				dw->average_rate + rate) /
				dw->average_points;
		speed = human_friendly_bytesize(dw->average_rate);
		if (dw->total_size) {
			float f;

			if (dw->average_rate > 0) {
				left = (dw->total_size - dw->received) /
						dw->average_rate;
				sprintf(time, "%u:%.2u", left / 60, left % 60);
			}

			/* convert to local encoding */
			err = utf8_to_local_encoding(
				messages_get("Download"), 0, &local_status);
			if (err != NSERROR_OK) {
				/* badenc should never happen */
				assert(err != NSERROR_BAD_ENCODING);
				/* hide nomem error */
				snprintf(dw->status, sizeof dw->status,
					messages_get("Download"),
					received, total_size, speed, time);
			}
			else {
				snprintf(dw->status, sizeof dw->status,
					local_status,
					received, total_size, speed, time);
				free(local_status);
			}

			f = (float) dw->received / (float) dw->total_size;
			width = download_progress_width * f;
		} else {
			left = t.tv_sec - dw->start_time.tv_sec;
			sprintf(time, "%u:%.2u", left / 60, left % 60);

			err = utf8_to_local_encoding(
				messages_get("DownloadU"), 0, &local_status);
			if (err != NSERROR_OK) {
				/* badenc should never happen */
				assert(err != NSERROR_BAD_ENCODING);
				/* hide nomem error */
				snprintf(dw->status, sizeof dw->status,
					messages_get("DownloadU"),
					received, speed, time);
			}
			else {
				snprintf(dw->status, sizeof dw->status,
					local_status,
					received, speed, time);
				free(local_status);
			}

			/* length unknown, stay at 0 til finished */
			width = 0;
		}
	} else {
		left = dw->last_time.tv_sec - dw->start_time.tv_sec;
		if (left == 0)
			left = 1;
		rate = (float) dw->received / (float) left;
		sprintf(time, "%u:%.2u", left / 60, left % 60);
		speed = human_friendly_bytesize(rate);

		err = utf8_to_local_encoding(messages_get("Downloaded"), 0,
				&local_status);
		if (err != NSERROR_OK) {
			/* badenc should never happen */
			assert(err != NSERROR_BAD_ENCODING);
			/* hide nomem error */
			snprintf(dw->status, sizeof dw->status,
				messages_get("Downloaded"),
				total_size, speed, time);
		}
		else {
			snprintf(dw->status, sizeof dw->status, local_status,
					total_size, speed, time);
			free(local_status);
		}

		/* all done */
		width = download_progress_width;
	}

	dw->last_time = t;
	dw->last_received = dw->received;

	error = xwimp_resize_icon(dw->window, ICON_DOWNLOAD_PROGRESS,
			download_progress_x0,
			download_progress_y0,
			download_progress_x0 + width,
			download_progress_y1);
	if (error) {
		LOG(("xwimp_resize_icon: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	error = xwimp_set_icon_state(dw->window, ICON_DOWNLOAD_STATUS, 0, 0);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	if (dw->ctx) {
		riscos_schedule(1000, ro_gui_download_update_status_wrapper, dw);
	} else {
		riscos_schedule(-1, ro_gui_download_update_status_wrapper, dw);
	}
}


/**
 * Wrapper for ro_gui_download_update_status(), suitable for riscos_schedule().
 */

void ro_gui_download_update_status_wrapper(void *p)
{
	ro_gui_download_update_status((struct gui_download_window *) p);
}



/**
 * Hide the caret but preserve input focus.
 *
 * \param  dw  download window
 */

void ro_gui_download_window_hide_caret(struct gui_download_window *dw)
{
	wimp_caret caret;
	os_error *error;

	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	else if (caret.w == dw->window) {
		error = xwimp_set_caret_position(dw->window, (wimp_i)-1, 0, 0, 1 << 25, -1);
		if (error) {
			LOG(("xwimp_get_caret_position: 0x%x : %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}




/**
 * Handle completed downloads.
 *
 * \param  dw  download window
 */

static void gui_download_window_done(struct gui_download_window *dw)
{
	os_error *error;

	if (dw->ctx != NULL)
		download_context_destroy(dw->ctx);
	dw->ctx = NULL;
	ro_gui_download_update_status(dw);

	error = xosfind_closew(dw->file);
	if (error) {
		LOG(("xosfind_closew: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
	}
	dw->file = 0;

	if (dw->saved) {
		error = xosfile_set_type(dw->path,
				dw->file_type);
		if (error) {
			LOG(("xosfile_set_type: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}

		if (dw->send_dataload) {
			ro_gui_download_send_dataload(dw);
		}

		riscos_schedule(2000, ro_gui_download_window_destroy_wrapper, dw);
	}
}


/**
 * Handle Mouse_Click events in a download window.
 *
 * \param  dw       download window
 * \param  pointer  block returned by Wimp_Poll
 */

bool ro_gui_download_click(wimp_pointer *pointer)
{
  	struct gui_download_window *dw;
	char command[256] = "Filer_OpenDir ";
	char *dot;
	os_error *error;

	dw = (struct gui_download_window *)ro_gui_wimp_event_get_user_data(pointer->w);
	if ((pointer->buttons & (wimp_DRAG_SELECT | wimp_DRAG_ADJUST)) &&
			pointer->i == ICON_DOWNLOAD_ICON && 
			!dw->error && !dw->saved) {
		const char *sprite = ro_gui_get_icon_string(pointer->w, pointer->i);
		int x = pointer->pos.x, y = pointer->pos.y;
		wimp_window_state wstate;
		wimp_icon_state istate;
		/* start the drag from the icon's exact location, rather than the pointer */
		istate.w = wstate.w = pointer->w;
		istate.i = pointer->i;
		if (!xwimp_get_window_state(&wstate) && !xwimp_get_icon_state(&istate)) {
			x = (istate.icon.extent.x1 + istate.icon.extent.x0)/2 +
					wstate.visible.x0 - wstate.xscroll;
			y = (istate.icon.extent.y1 + istate.icon.extent.y0)/2 +
					wstate.visible.y1 - wstate.yscroll;
		}
		ro_mouse_drag_start(ro_gui_download_drag_end, NULL, NULL, NULL);
		download_window_current = dw;
		ro_gui_drag_icon(x, y, sprite);

	} else if (pointer->i == ICON_DOWNLOAD_DESTINATION) {
		strncpy(command + 14, dw->path, 242);
		command[255] = 0;
		dot = strrchr(command, '.');
		if (dot) {
			*dot = 0;
			error = xos_cli(command);
			if (error) {
				LOG(("xos_cli: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
			}
		}
	}
	return true;
}


/**
 * Handler Key_Press events in a download window.
 *
 * \param  dw       download window
 * \param  key  key press returned by Wimp_Poll
 * \return true iff key press handled
 */

bool ro_gui_download_keypress(wimp_key *key)
{
  	struct gui_download_window *dw;

	dw = (struct gui_download_window *)ro_gui_wimp_event_get_user_data(key->w);
	switch (key->c)
	{
		case wimp_KEY_ESCAPE:
			ro_gui_download_window_destroy(dw, false);
			return true;

		case wimp_KEY_RETURN: {
			const char *name = ro_gui_get_icon_string(dw->window,
					ICON_DOWNLOAD_PATH);
			if (!strrchr(name, '.')) {
				warn_user("NoPathError", NULL);
				return true;
			}
			ro_gui_convert_save_path(dw->path, sizeof dw->path, name);

			dw->send_dataload = false;
			if (ro_gui_download_save(dw, dw->path,
					!nsoption_bool(confirm_overwrite)) && !dw->ctx)
			{
				/* finished already */
				riscos_schedule(2000, ro_gui_download_window_destroy_wrapper, dw);
			}
			return true;
		}
		break;
	}

	/* ignore all other keypresses (F12 etc) */
	return false;
}


/**
 * Handle User_Drag_Box event for a drag from a download window.
 *
 * \param *drag		block returned by Wimp_Poll
 * \param *data		NULL data to allow use as callback from ro_mouse.
 */

static void ro_gui_download_drag_end(wimp_dragged *drag, void *data)
{
	wimp_pointer pointer;
	wimp_message message;
	struct gui_download_window *dw = download_window_current;
	const char *leaf;
	os_error *error;

	if (dw->saved || dw->error)
		return;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* ignore drags to the download window itself */
	if (pointer.w == dw->window) return;

	leaf = strrchr(dw->path, '.');
	if (leaf)
		leaf++;
	else
		leaf = dw->path;
	ro_gui_convert_save_path(message.data.data_xfer.file_name, 212, leaf);

	message.your_ref = 0;
	message.action = message_DATA_SAVE;
	message.data.data_xfer.w = pointer.w;
	message.data.data_xfer.i = pointer.i;
	message.data.data_xfer.pos.x = pointer.pos.x;
	message.data.data_xfer.pos.y = pointer.pos.y;
	message.data.data_xfer.est_size = dw->total_size ? dw->total_size :
			dw->received;
	message.data.data_xfer.file_type = dw->file_type;
	message.size = 44 + ((strlen(message.data.data_xfer.file_name) + 4) &
			(~3u));

	error = xwimp_send_message_to_window(wimp_USER_MESSAGE, &message,
			pointer.w, pointer.i, 0);
	if (error) {
		LOG(("xwimp_send_message_to_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	
	gui_current_drag_type = GUI_DRAG_DOWNLOAD_SAVE;
}


/**
 * Handle Message_DataSaveAck for a drag from a download window.
 *
 * \param  message  block returned by Wimp_Poll
 */

void ro_gui_download_datasave_ack(wimp_message *message)
{
	struct gui_download_window *dw = download_window_current;

	dw->send_dataload = true;
	memcpy(&dw->save_message, message, sizeof(wimp_message));

	if (!ro_gui_download_save(dw, message->data.data_xfer.file_name,
			!nsoption_bool(confirm_overwrite)))
		return;

	if (!dw->ctx) {
		/* Ack successful completed save with message_DATA_LOAD immediately
		   to reduce the chance of the target app getting confused by it
		   being delayed */

		ro_gui_download_send_dataload(dw);

		riscos_schedule(2000, ro_gui_download_window_destroy_wrapper, dw);
	}
}


/**
 * Return a pathname in canonical form
 *
 * \param  path  pathnamee to be canonicalised
 * \return ptr to pathname in malloc block, or NULL
 */

char *ro_gui_download_canonicalise(const char *path)
{
	os_error *error;
	int spare = 0;
	char *buf;

	error = xosfscontrol_canonicalise_path(path, NULL, NULL, NULL, 0, &spare);
	if (error) {
		LOG(("xosfscontrol_canonicalise_path: 0x%x: %s",
			error->errnum, error->errmess));
		return NULL;
	}

	buf = malloc(1 - spare);
	if (buf) {
		error = xosfscontrol_canonicalise_path(path, buf, NULL, NULL,
				1 - spare, NULL);
		if (error) {
			LOG(("xosfscontrol_canonicalise_path: 0x%x: %s",
				error->errnum, error->errmess));

			free(buf);
			return NULL;
		}
	}

	return buf;
}


/**
 * Check the available space on the medium containing the destination file,
 * taking into account any space currently occupied by the file at its
 * original location.
 *
 * \param  dw         download window
 * \param  dest_file  destination pathname
 * \param  orig_file  current pathname, NULL if no existing file
 * \return true iff there's enough space
 */

bool ro_gui_download_check_space(struct gui_download_window *dw,
		const char *dest_file, const char *orig_file)
{
	/* is there enough free space for this file? */
	int dest_len = strlen(dest_file);
	os_error *error;
	int max_file;
	bits free_lo;
	int free_hi;
	char *dir;

	dir = malloc(dest_len + 1);
	if (!dir) return true;

	while (dest_len > 0 && dest_file[--dest_len] != '.');

	memcpy(dir, dest_file, dest_len);
	dir[dest_len] = '\0';

	/* try the 64-bit variant first (RO 3.6+) */
	error = xosfscontrol_free_space64(dir, &free_lo, &free_hi,
			&max_file, NULL, NULL);
	if (error) {
		LOG(("xosfscontrol_free_space64: 0x%x: %s",
			error->errnum, error->errmess));

		free_hi = 0;
		error = xosfscontrol_free_space(dir, (int*)&free_lo,
				&max_file, NULL);
		if (error) {
			LOG(("xosfscontrol_free_space: 0x%x: %s",
				error->errnum, error->errmess));
			/* close our eyes and hope */
			free(dir);
			return true;
		}
	}

	free(dir);

	if ((bits)max_file < dw->total_size || (!free_hi && free_lo < dw->total_size)) {
		char *dest_canon, *orig_canon;
		bits space;

		if (!orig_file || !dw->file) {
			/* no original file to take into account */
			return false;
		}

		space = min((bits)max_file, free_lo);

		dest_canon = ro_gui_download_canonicalise(dest_file);
		if (!dest_canon) dest_canon = (char*)dest_file;

		orig_canon = ro_gui_download_canonicalise(orig_file);
		if (!orig_canon) orig_canon = (char*)orig_file;

		/* not enough space; allow for the file's original location
		   when space is tight by comparing the first part of the two
		   pathnames (and assuming the FS isn't brain damaged!) */

		char *dot = strchr(orig_canon, '.');
		if (dot && !strncasecmp(dest_canon, orig_canon, (dot + 1) - orig_canon)) {
			int allocation;

			error = xosargs_read_allocation(dw->file,
					&allocation);
			if (error) {
				LOG(("xosargs_read_allocation: 0x%x : %s",
					error->errnum, error->errmess));
			}
			else {
				space += allocation;
			}
		}

		if (dest_canon != dest_file) free(dest_canon);
		if (orig_canon != orig_file) free(orig_canon);

		if (space >= dw->total_size) {
			/* OK, renaming should work */
			return true;
		}

		return false;
	}
	return true;
}

/**
 * Move the downloading file to a new location and continue downloading there.
 *
 * \param  dw         download window
 * \param  dest_file  new location
 * \param  src_file   old location
 * \return error iff failed to move file
 */

os_error *ro_gui_download_move(struct gui_download_window *dw,
		const char *dest_file, const char *src_file)
{
	os_error *error;

	/* close temporary file */
	if (dw->file) {
		error = xosfind_closew(dw->file);
		dw->file = 0;
		if (error) {
			LOG(("xosfind_closew: 0x%x: %s",
					error->errnum, error->errmess));
			return error;
		}
	}

	/* move or copy temporary file to destination file */
	error = xosfscontrol_rename(src_file, dest_file);
	/* Errors from a filing system have number 0x1XXnn, where XX is the FS
	 * number, and nn the error number. 0x9F is "Not same disc". */
	if (error && (error->errnum == error_BAD_RENAME ||
			(error->errnum & 0xFF00FFu) == 0x1009Fu)) {
		/* rename failed: copy with delete */
		error = xosfscontrol_copy(src_file, dest_file,
				osfscontrol_COPY_FORCE |
				osfscontrol_COPY_DELETE |
				osfscontrol_COPY_LOOK,
				0, 0, 0, 0, 0);
		if (error) {
			LOG(("xosfscontrol_copy: 0x%x: %s",
					error->errnum, error->errmess));
			return error;
		}
	} else if (error) {
		LOG(("xosfscontrol_rename: 0x%x: %s",
				error->errnum, error->errmess));
		return error;
	}

	if (dw->ctx) {
		/* open new destination file if still fetching */
		error = xosfile_write(dest_file, 0xdeaddead, 0xdeaddead,
				fileswitch_ATTR_OWNER_READ |
				fileswitch_ATTR_OWNER_WRITE);
		if (error) {
			LOG(("xosfile_write: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}

		error = xosfind_openupw(osfind_NO_PATH | osfind_ERROR_IF_DIR,
				dest_file, 0, &dw->file);
		if (error) {
			LOG(("xosfind_openupw: 0x%x: %s",
					error->errnum, error->errmess));
			return error;
		}

		error = xosargs_set_ptrw(dw->file, dw->received);
		if (error) {
			LOG(("xosargs_set_ptrw: 0x%x: %s",
					error->errnum, error->errmess));
			return error;
		}

	} else {
		/* otherwise just set the file type */
		error = xosfile_set_type(dest_file,
				dw->file_type);
		if (error) {
			LOG(("xosfile_set_type: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}
	}

	/* success */
	return NULL;
}


/**
 * Remember the directory containing the given file,
 * for use in further downloads.
 *
 * \param  path  pathname of downloaded file
 * \return none
 */

void ro_gui_download_remember_dir(const char *path)
{
	const char *lastdot = NULL;
	const char *p = path;

	while (*p >= 0x20) {
		if (*p == '.') {
			/* don't remember the directory if it's a temporary file */
			if (!lastdot && p == path + 12 &&
				!memcmp(path, "<Wimp$Scrap>", 12)) break;
			lastdot = p;
		}
		p++;
	}

	if (lastdot) {
		/* remember the directory */
		char *new_dir = realloc(download_dir, (lastdot+1)-path);
		if (new_dir) {
			download_dir_len = lastdot - path;
			memcpy(new_dir, path, download_dir_len);
			new_dir[download_dir_len] = '\0';
			download_dir = new_dir;
		}
	}
}

/**
 * Start of save operation, user has specified where the file should be saved.
 *
 * \param  dw               download window
 * \param  file_name        pathname of destination file
 & \param  force_overwrite  true iff required to overwrite without prompting
 * \return true iff save successfully initiated
 */

bool ro_gui_download_save(struct gui_download_window *dw,
		const char *file_name, bool force_overwrite)
{
	fileswitch_object_type obj_type;
	const char *temp_name;
	os_error *error;

	if (dw->saved || dw->error)
		return true;

	temp_name = ro_gui_download_temp_name(dw);

	/* does the user want to check for collisions when saving? */
	if (!force_overwrite) {
		/* check whether the destination file/dir already exists */
		error = xosfile_read_stamped(file_name, &obj_type,
				NULL, NULL, NULL, NULL, NULL);
		if (error) {
			LOG(("xosfile_read_stamped: 0x%x:%s", error->errnum, error->errmess));
			return false;
		}

		switch (obj_type) {
			case osfile_NOT_FOUND:
				break;

			case osfile_IS_FILE:
				dw->query = query_user("OverwriteFile", NULL, &overwrite_funcs, dw,
							messages_get("Replace"), messages_get("DontReplace"));
				dw->query_rsn = QueryRsn_Overwrite;
				return false;

			default:
				error = xosfile_make_error(file_name, obj_type);
				assert(error);
				warn_user("SaveError", error->errmess);
				return false;
		}
	}

	if (!ro_gui_download_check_space(dw, file_name, temp_name)) {
		warn_user("SaveError", messages_get("NoDiscSpace"));
		return false;
	}

	error = ro_gui_download_move(dw, file_name, temp_name);
	if (error) {
		warn_user("SaveError", error->errmess);

		/* try to reopen at old location so that the download can continue
		   to the temporary file */
		error = xosfind_openupw(osfind_NO_PATH | osfind_ERROR_IF_DIR,
				temp_name, 0, &dw->file);
		if (error) {
			LOG(("xosfind_openupw: 0x%x: %s",
					error->errnum, error->errmess));

		} else {
			error = xosargs_set_ptrw(dw->file, dw->received);
			if (error) {
				LOG(("xosargs_set_ptrw: 0x%x: %s",
						error->errnum, error->errmess));
			}
		}

		if (error) {
			if (dw->ctx)
				download_context_abort(dw->ctx);
			gui_download_window_error(dw, error->errmess);
		}
		return false;
	}

	dw->saved = true;
	strncpy(dw->path, file_name, sizeof dw->path);

	if (!dw->send_dataload || dw->save_message.data.data_xfer.est_size != -1)
		ro_gui_download_remember_dir(file_name);

	/* grey out file icon */
	error = xwimp_set_icon_state(dw->window, ICON_DOWNLOAD_ICON,
			wimp_ICON_SHADED, wimp_ICON_SHADED);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	/* hide writeable path icon and show destination icon
	   Note: must redraw icon bounding box because the destination icon
		has rounded edges on RISC OS Select/Adjust and doesn't
		completely cover the writeable icon */

	ro_gui_force_redraw_icon(dw->window, ICON_DOWNLOAD_PATH);
	error = xwimp_set_icon_state(dw->window, ICON_DOWNLOAD_PATH,
			wimp_ICON_DELETED, wimp_ICON_DELETED);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	error = xwimp_set_icon_state(dw->window,
			ICON_DOWNLOAD_DESTINATION, wimp_ICON_DELETED, 0);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	ro_gui_download_window_hide_caret(dw);

	return true;
}


/**
 * Send DataLoad message in response to DataSaveAck, informing the
 * target application that the transfer is complete.
 *
 * \param  dw  download window
 */

void ro_gui_download_send_dataload(struct gui_download_window *dw)
{
	/* Ack successful save with message_DATA_LOAD */
	wimp_message *message = &dw->save_message;
	os_error *error;

	assert(dw->send_dataload);
	dw->send_dataload = false;

	message->action = message_DATA_LOAD;
	message->your_ref = message->my_ref;
	error = xwimp_send_message_to_window(wimp_USER_MESSAGE, message,
			message->data.data_xfer.w,
			message->data.data_xfer.i, 0);
	/* The window we just attempted to send a message to may
	 * have been closed before the message was sent. As we've
	 * no clean way of detecting this, we'll just detect the
	 * error return from the message send attempt and judiciously
	 * ignore it.
	 *
	 * Ideally, we would have registered to receive Message_WindowClosed
	 * and then cleared dw->send_dataload flag for the appropriate
	 * window. Unfortunately, however, a long-standing bug in the
	 * Pinboard module prevents this from being a viable solution.
	 *
	 * See http://groups.google.co.uk/group/comp.sys.acorn.tech/msg/e3fbf70d8393e6cf?dmode=source&hl=en
	 * for the rather depressing details.
	 */
	if (error && error->errnum != error_WIMP_BAD_HANDLE) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	riscos_schedule(2000, ro_gui_download_window_destroy_wrapper, dw);
}


/**
 * Handle closing of download window
 */
void ro_gui_download_close(wimp_w w)
{
	struct gui_download_window *dw;

	dw = (struct gui_download_window *)ro_gui_wimp_event_get_user_data(w);
	ro_gui_download_window_destroy(dw, false);
}


/**
 * Close a download window and free any related resources.
 *
 * \param  dw   download window
 * \param  quit destroying because we're quitting the whole app
 * \return true if window destroyed, not waiting for user confirmation
 */

bool ro_gui_download_window_destroy(struct gui_download_window *dw, bool quit)
{
	bool safe = dw->saved && !dw->ctx;
	os_error *error;

	if (!safe && !dw->close_confirmed)
	{
		query_reason rsn = quit ? QueryRsn_Quit : QueryRsn_Abort;

		if (dw->query != QUERY_INVALID) {

			/* can we just reuse the existing query? */
			if (rsn == dw->query_rsn) {
				ro_gui_query_window_bring_to_front(dw->query);
				return false;
			}

			query_close(dw->query);
			dw->query = QUERY_INVALID;
		}

		if (quit) {
			/* bring all download windows to the front of the desktop as
			   a convenience if there are lots of windows open */

			struct gui_download_window *d = download_window_list;
			while (d) {
				ro_gui_dialog_open_top(d->window, NULL, 0, 0);
				d = d->next;
			}
		}

		dw->query_rsn = rsn;
		dw->query = query_user(quit ? "QuitDownload" : "AbortDownload",
				NULL, &close_funcs, dw, NULL, NULL);

		return false;
	}

	riscos_schedule(-1, ro_gui_download_update_status_wrapper, dw);
	riscos_schedule(-1, ro_gui_download_window_destroy_wrapper, dw);

	/* remove from list */
	if (dw->prev)
		dw->prev->next = dw->next;
	else
		download_window_list = dw->next;
	if (dw->next)
		dw->next->prev = dw->prev;

	/* delete window */
	error = xwimp_delete_window(dw->window);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	ro_gui_wimp_event_finalise(dw->window);

	/* close download file */
	if (dw->file) {
		error = xosfind_closew(dw->file);
		if (error) {
			LOG(("xosfind_closew: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}
	}

	/* delete temporary file */
	if (!dw->saved) {
		const char *temp_name = ro_gui_download_temp_name(dw);

		error = xosfile_delete(temp_name, 0, 0, 0, 0, 0);
		if (error) {
			LOG(("xosfile_delete: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}
	}

	if (dw->ctx) {
		download_context_abort(dw->ctx);
		download_context_destroy(dw->ctx);
	}

	free(dw);

	return true;
}


/**
 * Wrapper for ro_gui_download_window_destroy(), suitable for riscos_schedule().
 */

void ro_gui_download_window_destroy_wrapper(void *p)
{
	struct gui_download_window *dw = p;
	if (dw->query != QUERY_INVALID)
		query_close(dw->query);
	dw->query = QUERY_INVALID;
	dw->close_confirmed = true;
	ro_gui_download_window_destroy(dw, false);
}


/**
 * User has opted to cancel the close, leaving the download to continue.
 */

void ro_gui_download_close_cancelled(query_id id, enum query_response res, void *p)
{
	struct gui_download_window *dw = p;
	dw->query = QUERY_INVALID;
}


/**
 * Download aborted, close window and tidy up.
 */

void ro_gui_download_close_confirmed(query_id id, enum query_response res, void *p)
{
	struct gui_download_window *dw = p;
	dw->query = QUERY_INVALID;
	dw->close_confirmed = true;
	if (dw->query_rsn == QueryRsn_Quit) {

		/* destroy all our downloads */
		while (download_window_list)
			ro_gui_download_window_destroy_wrapper(download_window_list);

		/* and restart the shutdown */
		if (ro_gui_prequit())
			netsurf_quit = true;
	}
	else
		ro_gui_download_window_destroy(dw, false);
}


/**
 * User has opted not to overwrite the existing file.
 */

void ro_gui_download_overwrite_cancelled(query_id id, enum query_response res, void *p)
{
	struct gui_download_window *dw = p;
	dw->query = QUERY_INVALID;
}


/**
 * Overwrite of existing file confirmed, proceed with the save.
 */

void ro_gui_download_overwrite_confirmed(query_id id, enum query_response res, void *p)
{
	struct gui_download_window *dw = p;
	dw->query = QUERY_INVALID;

	if (!ro_gui_download_save(dw, dw->save_message.data.data_xfer.file_name, true))
		return;

	if (!dw->ctx) {
		/* Ack successful completed save with message_DATA_LOAD immediately
		   to reduce the chance of the target app getting confused by it
		   being delayed */

		ro_gui_download_send_dataload(dw);

		riscos_schedule(2000, ro_gui_download_window_destroy_wrapper, dw);
	}
}


/**
 * Respond to PreQuit message, displaying a prompt message if we need
 * the user to confirm the shutdown.
 *
 * \return true if we can shutdown straightaway
 */

bool ro_gui_download_prequit(void)
{
	while (download_window_list)
	{
		if (!ro_gui_download_window_destroy(download_window_list, true))
			return false;	/* awaiting user confirmation */
	}
	return true;
}

static struct gui_download_table download_table = {
	.create = gui_download_window_create,
	.data = gui_download_window_data,
	.error = gui_download_window_error,
	.done = gui_download_window_done,
};

struct gui_download_table *riscos_download_table = &download_table;
