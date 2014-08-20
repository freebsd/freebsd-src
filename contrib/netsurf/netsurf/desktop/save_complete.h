/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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
 * Save HTML document with dependencies (interface).
 */

#ifndef _NETSURF_DESKTOP_SAVE_COMPLETE_H_
#define _NETSURF_DESKTOP_SAVE_COMPLETE_H_

#include <stdbool.h>

#include <libwapcaplet/libwapcaplet.h>

struct hlcache_handle;

/**
 * Callback to set type of a file
 *
 * \param path       Native path of file
 * \param mime_type  MIME type of file content
 */
typedef void (*save_complete_set_type_cb)(const char *path,
		lwc_string *mime_type);

/**
 * Initialise save complete module.
 */
void save_complete_init(void);

/**
 * Save an HTML page with all dependencies.
 *
 * \param  c         CONTENT_HTML to save
 * \param  path      Native path to directory to save in to (must exist)
 * \param  set_type  Callback to set type of a file, or NULL
 * \return  true on success, false on error and error reported
 */
bool save_complete(struct hlcache_handle *c, const char *path,
		save_complete_set_type_cb set_type);

#endif
