/*
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2010 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * SSL Certificate verification UI (implementation)
 */

#include "utils/config.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "oslib/wimp.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/fetch.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/sslcert_viewer.h"
#include "desktop/gui.h"
#include "desktop/tree.h"
#include "riscos/dialog.h"
#include "riscos/sslcert.h"
#include "riscos/textarea.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "utils/log.h"
#include "utils/utils.h"

#define ICON_SSL_PANE 1
#define ICON_SSL_REJECT 3
#define ICON_SSL_ACCEPT 4

static wimp_window *ro_gui_cert_dialog_template;
static wimp_window *ro_gui_cert_tree_template;

struct ro_sslcert
{
	wimp_w				window;
	wimp_w				pane;
	ro_treeview			*tv;
	struct sslcert_session_data	*data;
};

static void ro_gui_cert_accept(wimp_pointer *pointer);
static void ro_gui_cert_reject(wimp_pointer *pointer);
static void ro_gui_cert_close_window(wimp_w w);
static void ro_gui_cert_release_window(struct ro_sslcert *s);

/**
 * Load and initialise the certificate window template
 */

void ro_gui_cert_preinitialise(void)
{
	/* Load templates for the SSL windows and adjust the tree window
	 * flags to suit.
	 */

	ro_gui_cert_dialog_template = ro_gui_dialog_load_template("sslcert");
	ro_gui_cert_tree_template = ro_gui_dialog_load_template("tree");

	ro_gui_cert_tree_template->flags &= ~(wimp_WINDOW_MOVEABLE |
			wimp_WINDOW_BACK_ICON |
			wimp_WINDOW_CLOSE_ICON |
			wimp_WINDOW_TITLE_ICON |
			wimp_WINDOW_SIZE_ICON |
			wimp_WINDOW_TOGGLE_ICON);
}

/**
 * Load and initialise the certificate window template
 */

void ro_gui_cert_postinitialise(void)
{
	/* Initialise the SSL module. */
}

/**
 * Open the certificate verification dialog
 *
 * \param  *bw			The browser window owning the certificates.
 * \param  *c			The content data corresponding to the
 *				certificates.
 * \param  *certs		The certificate details.
 * \param  num			The number of certificates included.
 */

void gui_cert_verify(nsurl *url,
		const struct ssl_cert_info *certs, unsigned long num,
		nserror (*cb)(bool proceed, void *pw), void *cbpw)
{
	struct ro_sslcert		*sslcert_window;
	wimp_window_state		state;
	wimp_icon_state			istate;
	wimp_window_info		info;
	os_error			*error;
	bool				set_extent;

	assert(certs);

	sslcert_window = malloc(sizeof(struct ro_sslcert));
	if (sslcert_window == NULL) {
		LOG(("Failed to allocate memory for SSL Cert Dialog"));
		return;
	}

	/* Create the SSL window and its pane. */

	error = xwimp_create_window(ro_gui_cert_dialog_template,
			&(sslcert_window->window));
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		free(sslcert_window);
		return;
	}

	error = xwimp_create_window(ro_gui_cert_tree_template,
			&(sslcert_window->pane));
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		free(sslcert_window);
		return;
	}

	/* Create the SSL data and build a tree from it. */
	sslcert_viewer_create_session_data(num, url,
			cb, cbpw, certs, &sslcert_window->data);
	ssl_current_session = sslcert_window->data;

	sslcert_window->tv = ro_treeview_create(sslcert_window->pane,
			NULL, NULL, TREE_SSLCERT);
	if (sslcert_window->tv == NULL) {
		LOG(("Failed to allocate treeview"));
		free(sslcert_window);
		return;
	}

	/* Set up the certificate window event handling.
	 *
	 * (The action buttons are registered as button events, not OK and
	 * Cancel, as both need to carry out actions.)
	 */

	ro_gui_wimp_event_set_user_data(sslcert_window->window, sslcert_window);
	ro_gui_wimp_event_register_close_window(sslcert_window->window,
			ro_gui_cert_close_window);
	ro_gui_wimp_event_register_button(sslcert_window->window,
			ICON_SSL_REJECT, ro_gui_cert_reject);
	ro_gui_wimp_event_register_button(sslcert_window->window,
			ICON_SSL_ACCEPT, ro_gui_cert_accept);

	ro_gui_dialog_open_persistent(NULL, sslcert_window->window, false);

	/* Nest the tree window inside the pane window.  To do this, we:
	 * - Get the current pane extent,
	 * - Get the parent window position and the location of the pane-
	 *   locating icon inside it,
	 * - Set the visible area of the pane to suit,
	 * - Check that the pane extents are OK for this visible area, and
	 *   increase them if necessary,
	 * - Before finally opening the pane as a nested part of the parent.
	 */

	info.w = sslcert_window->pane;
	error = xwimp_get_window_info_header_only(&info);
	if (error) {
		ro_gui_cert_release_window(sslcert_window);
		LOG(("xwimp_get_window_info: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	state.w = sslcert_window->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		ro_gui_cert_release_window(sslcert_window);
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	istate.w = sslcert_window->window;
	istate.i = ICON_SSL_PANE;
	error = xwimp_get_icon_state(&istate);
	if (error) {
		ro_gui_cert_release_window(sslcert_window);
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	state.w = sslcert_window->pane;
	state.visible.x1 = state.visible.x0 + istate.icon.extent.x1 - 20 -
			ro_get_vscroll_width(sslcert_window->pane);
	state.visible.x0 += istate.icon.extent.x0 + 20;
	state.visible.y0 = state.visible.y1 + istate.icon.extent.y0 + 20 +
			ro_get_hscroll_height(sslcert_window->pane);
	state.visible.y1 += istate.icon.extent.y1 - 32;

	set_extent = false;

	if ((info.extent.x1 - info.extent.x0) <
			(state.visible.x1 - state.visible.x0)) {
		info.extent.x0 = 0;
		info.extent.x1 = state.visible.x1 - state.visible.x0;
		set_extent = true;
	}
	if ((info.extent.y1 - info.extent.y0) <
			(state.visible.y1 - state.visible.y0)) {
		info.extent.y1 = 0;
		info.extent.x1 = state.visible.y0 - state.visible.y1;
		set_extent = true;
	}

	if (set_extent) {
		error = xwimp_set_extent(sslcert_window->pane, &(info.extent));
		if (error) {
			ro_gui_cert_release_window(sslcert_window);
			LOG(("xwimp_set_extent: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}
	}

	error = xwimp_open_window_nested(PTR_WIMP_OPEN(&state),
			sslcert_window->window,
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_XORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_YORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_LS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_RS_EDGE_SHIFT);
	if (error) {
		ro_gui_cert_release_window(sslcert_window);
		LOG(("xwimp_open_window_nested: 0x%x: %s",
				error->errnum, error->errmess));
		ro_gui_cert_release_window(sslcert_window);
		return;
	}

	ro_treeview_set_origin(sslcert_window->tv, 0, 0);
}

/**
 * Handle acceptance of certificate via event callback.
 *
 * \param  *pointer		The wimp pointer block.
 */

void ro_gui_cert_accept(wimp_pointer *pointer)
{
	struct ro_sslcert *s;

	s = (struct ro_sslcert *) ro_gui_wimp_event_get_user_data(pointer->w);

	if (s != NULL) {
		sslcert_viewer_accept(s->data);
		ro_gui_dialog_close(s->window);
		ro_gui_cert_release_window(s);
	}
}

/**
 * Handle rejection of certificate via event callback.
 *
 * \param  w		The wimp pointer block.
 */

void ro_gui_cert_reject(wimp_pointer *pointer)
{
	struct ro_sslcert *s;

	s = (struct ro_sslcert *) ro_gui_wimp_event_get_user_data(pointer->w);

	if (s != NULL) {
		sslcert_viewer_reject(s->data);
		ro_gui_dialog_close(s->window);
		ro_gui_cert_release_window(s);
	}
}

/**
 * Callback to handle the closure of the SSL dialogue by other means.
 *
 * \param w		The window being closed.
 */

static void ro_gui_cert_close_window(wimp_w w)
{
	struct ro_sslcert *s;

	s = (struct ro_sslcert *) ro_gui_wimp_event_get_user_data(w);

	if (s != NULL)
		ro_gui_cert_release_window(s);
}

/**
 * Handle closing of the RISC OS certificate verification dialog, deleting
 * the windows and freeing up the treeview and data block.
 *
 * \param  *s			The data block associated with the dialogue.
 */

void ro_gui_cert_release_window(struct ro_sslcert *s)
{
	os_error *error;

	if (s == NULL)
		return;

	LOG(("Releasing SSL data: 0x%x", (unsigned) s));

	ro_gui_wimp_event_finalise(s->window);
	ro_treeview_destroy(s->tv);

	error = xwimp_delete_window(s->window);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x:%s",
		     error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	error = xwimp_delete_window(s->pane);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x:%s",
		     error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	free(s);
}

