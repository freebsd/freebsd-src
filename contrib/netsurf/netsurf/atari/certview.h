/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
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

#ifndef CERTVIEW_H_INCLUDED
#define CERTVIEW_H_INCLUDED

#include <inttypes.h>
#include <sys/types.h>
#include <string.h>


#include "assert.h"
#include "utils/nsoption.h"
#include "utils/log.h"
#include "desktop/sslcert_viewer.h"

struct core_window;

struct atari_sslcert_viewer_s {
	GUIWIN * window;
	//struct atari_treeview_window *tv;/*< The hotlist treeview handle.  */
	struct core_window *tv;
	struct sslcert_session_data *ssl_session_data;
	bool init;
};

/**
 * Initializes and opens an certificate inspector window
 * \param  ssl_d ssl session data created by sslcert_viewer_create_session_data
 *
 * The window takes ownership of the session data and free's the memory on exit.
 */
void atari_sslcert_viewer_open(struct sslcert_session_data *ssl_d);


#endif // CERTVIEW_H_INCLUDED
