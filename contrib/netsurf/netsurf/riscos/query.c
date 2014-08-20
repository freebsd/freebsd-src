/*
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

#include <stdlib.h>
#include <string.h>

#include <stdbool.h>

#include "riscos/dialog.h"
#include "riscos/query.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "riscos/ucstables.h"

#define ICON_QUERY_MESSAGE 0
#define ICON_QUERY_YES 1
#define ICON_QUERY_NO 2
#define ICON_QUERY_HELP 3

/** Data for a query window */
struct gui_query_window
{
	struct gui_query_window *prev;	/** Previous query in list */
	struct gui_query_window *next;	/** Next query in list */

	query_id id;	/** unique ID number for this query */
	wimp_w window;	/** RISC OS window handle */

	const query_callback *cb;	/** Table of callback functions */
	void *pw;	/** Handle passed to callback functions */

	bool default_confirm;	/** Default action is to confirm */
};


/** Next unallocated query id */
static query_id next_id = (query_id)1;

/** List of all query windows. */
static struct gui_query_window *gui_query_window_list = 0;

/** Template for a query window. */
static struct wimp_window *query_template;

/** Widths of Yes and No buttons */
static int query_yes_width = 0;
static int query_no_width  = 0;

static struct gui_query_window *ro_gui_query_window_lookup_id(query_id id);

static bool ro_gui_query_click(wimp_pointer *pointer);
static void ro_gui_query_close(wimp_w w);
static bool ro_gui_query_apply(wimp_w w);


void ro_gui_query_init(void)
{
	query_template = ro_gui_dialog_load_template("query");
}


/**
 * Lookup a query window using its ID number
 *
 * \param  id  id to search for
 * \return pointer to query window or NULL
 */

struct gui_query_window *ro_gui_query_window_lookup_id(query_id id)
{
	struct gui_query_window *qw = gui_query_window_list;
	while (qw && qw->id != id)
		qw = qw->next;
	return qw;
}


/**
 * Display a query to the user, requesting a response, near the current
 * pointer position to keep the required mouse travel small, but also
 * protecting against spurious mouse clicks.
 *
 * \param  query   message token of query
 * \param  detail  parameter used in expanding tokenised message
 * \param  cb      table of callback functions to be called when user responds
 * \param  pw      handle to be passed to callback functions
 * \param  yes     text to use for 'Yes' button' (or NULL for default)
 * \param  no      text to use for 'No' button (or NULL for default)
 * \return id number of the query (or QUERY_INVALID if it failed)
 */

query_id query_user(const char *query, const char *detail,
		const query_callback *cb, void *pw,
		const char *yes, const char *no)
{
	wimp_pointer pointer;
	if (xwimp_get_pointer_info(&pointer))
		pointer.pos.y = pointer.pos.x = -1;

	return query_user_xy(query, detail, cb, pw, yes, no,
				pointer.pos.x, pointer.pos.y);
}


/**
 * Display a query to the user, requesting a response, at a specified
 * screen position (x,y). The window is positioned relative to the given
 * location such that the required mouse travel is small, but non-zero
 * for protection spurious double-clicks.
 *
 * \param  query   message token of query
 * \param  detail  parameter used in expanding tokenised message
 * \param  cb      table of callback functions to be called when user responds
 * \param  pw      handle to be passed to callback functions
 * \param  yes     text to use for 'Yes' button' (or NULL for default)
 * \param  no      text to use for 'No' button (or NULL for default)
 * \param  x       x position in screen coordinates (-1 = centred on screen)
 * \param  y       y position in screen coordinates (-1 = centred on screen)
 * \return id number of the query (or QUERY_INVALID if it failed)
 */

query_id query_user_xy(const char *query, const char *detail,
		const query_callback *cb, void *pw,
		const char *yes, const char *no,
		int x, int y)
{
	struct gui_query_window *qw;
	char query_buffer[300];
	os_error *error;
	wimp_icon *icn;
	int width;
	int len;
	int tx;
	char *local_text = NULL;
	nserror err;

	qw = malloc(sizeof(struct gui_query_window));
	if (!qw) {
		warn_user("NoMemory", NULL);
		return QUERY_INVALID;
	}

	qw->cb = cb;
	qw->pw = pw;
	qw->id = next_id++;
	qw->default_confirm = false;

	if (next_id == QUERY_INVALID)
		next_id++;

	if (!yes) yes = messages_get("Yes");
	if (!no) no = messages_get("No");

	/* set the text of the 'Yes' button and size accordingly */
	err = utf8_to_local_encoding(yes, 0, &local_text);
	if (err != NSERROR_OK) {
		assert(err != NSERROR_BAD_ENCODING);
		LOG(("utf8_to_local_encoding_failed"));
		local_text = NULL;
	}

	icn = &query_template->icons[ICON_QUERY_YES];
	len = strlen(local_text ? local_text : yes);
	len = max(len, icn->data.indirected_text.size - 1);
	memcpy(icn->data.indirected_text.text,
			local_text ? local_text: yes, len);
	icn->data.indirected_text.text[len] = '\0';

	free(local_text);
	local_text = NULL;

	error = xwimptextop_string_width(icn->data.indirected_text.text, len, &width);
	if (error) {
		LOG(("xwimptextop_string_width: 0x%x:%s",
			error->errnum, error->errmess));
		width = len * 16;
	}
	if (!query_yes_width) query_yes_width = icn->extent.x1 - icn->extent.x0;
	width += 44;
	if (width < query_yes_width)
		width = query_yes_width;
	icn->extent.x0 = tx = icn->extent.x1 - width;

	/* set the text of the 'No' button and size accordingly */
	err = utf8_to_local_encoding(no, 0, &local_text);
	if (err != NSERROR_OK) {
		assert(err != NSERROR_BAD_ENCODING);
		LOG(("utf8_to_local_encoding_failed"));
		local_text = NULL;
	}

	icn = &query_template->icons[ICON_QUERY_NO];
	len = strlen(local_text ? local_text : no);
	len = max(len, icn->data.indirected_text.size - 1);
	memcpy(icn->data.indirected_text.text,
			local_text ? local_text : no, len);
	icn->data.indirected_text.text[len] = '\0';

	free(local_text);
	local_text = NULL;

	if (!query_no_width) query_no_width = icn->extent.x1 - icn->extent.x0;
	icn->extent.x1 = tx - 16;
	error = xwimptextop_string_width(icn->data.indirected_text.text, len, &width);
	if (error) {
		LOG(("xwimptextop_string_width: 0x%x:%s",
			error->errnum, error->errmess));
		width = len * 16;
	}
	width += 28;
	if (width < query_no_width)
		width = query_no_width;
	icn->extent.x0 = icn->extent.x1 - width;

	error = xwimp_create_window(query_template, &qw->window);
	if (error) {
		warn_user("WimpError", error->errmess);
		free(qw);
		return QUERY_INVALID;
	}

	snprintf(query_buffer, sizeof query_buffer, "%s %s",
			messages_get(query), detail ? detail : "");
	query_buffer[sizeof query_buffer - 1] = 0;

	ro_gui_set_icon_string(qw->window, ICON_QUERY_MESSAGE,
			query_buffer, true);

	xwimp_set_icon_state(qw->window, ICON_QUERY_HELP,
			wimp_ICON_DELETED, wimp_ICON_DELETED);

	if (x >= 0 && y >= 0) {
		x -= tx - 8;
		y += (query_template->visible.y1 - query_template->visible.y0) / 2;
		ro_gui_dialog_open_xy(qw->window, x, y);
	}
	else
		ro_gui_dialog_open(qw->window);

	ro_gui_wimp_event_set_user_data(qw->window, qw);
	ro_gui_wimp_event_register_mouse_click(qw->window, ro_gui_query_click);
	ro_gui_wimp_event_register_cancel(qw->window, ICON_QUERY_NO);
	ro_gui_wimp_event_register_ok(qw->window, ICON_QUERY_YES, ro_gui_query_apply);
	ro_gui_wimp_event_register_close_window(qw->window, ro_gui_query_close);

	error = xwimp_set_caret_position(qw->window, (wimp_i)-1, 0, 0, 1 << 25, -1);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	/* put this query window at the head of our list */
	if (gui_query_window_list)
		gui_query_window_list->prev = qw;

	qw->prev = NULL;
	qw->next = gui_query_window_list;
	gui_query_window_list = qw;

	return qw->id;
}


/**
 * Close a query window without waiting for a response from the user.
 * (should normally only be called if the user has responded in some other
 *  way of which the query window in unaware.)
 *
 * \param  id  id of query window to close
 */

void query_close(query_id id)
{
	struct gui_query_window *qw = ro_gui_query_window_lookup_id(id);
	if (!qw)
		return;
	ro_gui_query_close(qw->window);

}


void ro_gui_query_window_bring_to_front(query_id id)
{
	struct gui_query_window *qw = ro_gui_query_window_lookup_id(id);
	if (qw) {
		os_error *error;

		ro_gui_dialog_open(qw->window);

		error = xwimp_set_caret_position(qw->window, (wimp_i)-1, 0, 0, 1 << 25, -1);
		if (error) {
			LOG(("xwimp_get_caret_position: 0x%x : %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}


/**
 * Handle closing of query dialog
 */
void ro_gui_query_close(wimp_w w)
{
	struct gui_query_window *qw;
	os_error *error;

	qw = (struct gui_query_window *)ro_gui_wimp_event_get_user_data(w);

	ro_gui_dialog_close(w);
	error = xwimp_delete_window(qw->window);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	ro_gui_wimp_event_finalise(w);

	/* remove from linked-list of query windows and release memory */
	if (qw->prev)
		qw->prev->next = qw->next;
	else
		gui_query_window_list = qw->next;

	if (qw->next)
		qw->next->prev = qw->prev;
	free(qw);
}


/**
 * Handle acceptance of query dialog
 */
bool ro_gui_query_apply(wimp_w w)
{
	struct gui_query_window *qw;
	const query_callback *cb;

	qw = (struct gui_query_window *)ro_gui_wimp_event_get_user_data(w);
	cb = qw->cb;
	cb->confirm(qw->id, QUERY_YES, qw->pw);
	return true;
}


/**
 * Handle clicks in query dialog
 */
bool ro_gui_query_click(wimp_pointer *pointer)
{
	struct gui_query_window *qw;
	const query_callback *cb;

	qw = (struct gui_query_window *)ro_gui_wimp_event_get_user_data(pointer->w);
	cb = qw->cb;

	switch (pointer->i) {
		case ICON_QUERY_NO:
			cb->cancel(qw->id, QUERY_NO, qw->pw);
			break;
		default:
			return false;
	}
	return false;
}
