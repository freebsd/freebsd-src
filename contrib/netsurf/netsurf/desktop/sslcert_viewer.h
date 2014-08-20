/*
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>
 * Copyright 2013 Michael Drake <tlsa@netsurf-browser.org>
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


#ifndef _NETSURF_DESKTOP_SSLCERT_VIEWER_H_
#define _NETSURF_DESKTOP_SSLCERT_VIEWER_H_

#include <stdbool.h>
#include <stdint.h>

#include "desktop/browser.h"
#include "desktop/core_window.h"
#include "desktop/textinput.h"
#include "utils/errors.h"

struct sslcert_session_data;


/**
 * Create ssl certificate viewer session data.
 *
 * \param num		The number of certificates in the chain
 * \param url		Address of the page we're inspecting certificates of
 * \param cb		Low level cache callback
 * \param cbpw		Low level cache private data
 * \param certs		The SSL certificates
 * \param ssl_d		Updated to SSL certificate session data
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * Pass the session data to sslcert_viewer_init.
 * sslcert_viewer_fini destroys the session data.
 */
nserror sslcert_viewer_create_session_data(
		unsigned long num, nsurl *url, llcache_query_response cb,
		void *cbpw, const struct ssl_cert_info *certs,
		struct sslcert_session_data **ssl_d);

/**
 * Initialise a ssl certificate viewer from session data.
 *
 * This iterates through the certificates, building a treeview.
 *
 * \param cw_t		Callback table for cert viewer's core_window
 * \param cw		The core_window in which the cert viewer is shown
 * \param ssl_d		SSL certificate session data
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror sslcert_viewer_init(struct core_window_callback_table *cw_t,
		void *core_window_handle, struct sslcert_session_data *ssl_d);

/**
 * Finalise a ssl certificate viewer.
 *
 * This destroys the certificate treeview and the certificate viewer module's
 * session data.
 *
 * \param ssl_d		SSL certificate session data
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror sslcert_viewer_fini(struct sslcert_session_data *ssl_d);

/**
 * Reject a certificate chain.
 *
 * \param ssl_d		SSL certificate session data
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror sslcert_viewer_reject(struct sslcert_session_data *ssl_d);

/**
 * Accept a certificate chain.
 *
 * \param ssl_d		SSL certificate session data
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror sslcert_viewer_accept(struct sslcert_session_data *ssl_d);

/**
 * Redraw the ssl certificate viewer.
 *
 * \param ssl_d		SSL certificate session data
 * \param x		X coordinate to render treeview at
 * \param x		Y coordinate to render treeview at
 * \param clip		Current clip rectangle (wrt tree origin)
 * \param ctx		Current redraw context
 */
void sslcert_viewer_redraw(struct sslcert_session_data *ssl_d,
		int x, int y, struct rect *clip,
		const struct redraw_context *ctx);

/**
 * Handles all kinds of mouse action
 *
 * \param ssl_d		SSL certificate session data
 * \param mouse		The current mouse state
 * \param x		X coordinate
 * \param y		Y coordinate
 */
void sslcert_viewer_mouse_action(struct sslcert_session_data *ssl_d,
		browser_mouse_state mouse, int x, int y);

/**
 * Key press handling.
 *
 * \param ssl_d		SSL certificate session data
 * \param key		The ucs4 character codepoint
 * \return true if the keypress is dealt with, false otherwise.
 */
void sslcert_viewer_keypress(struct sslcert_session_data *ssl_d,
		uint32_t key);

#endif
