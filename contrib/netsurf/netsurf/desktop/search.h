/*
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

#ifndef _NETSURF_DESKTOP_SEARCH_H_
#define _NETSURF_DESKTOP_SEARCH_H_

#include <ctype.h>
#include <string.h>

struct browser_window;

typedef enum {
	SEARCH_FLAG_NONE		= 0,
	SEARCH_FLAG_CASE_SENSITIVE 	= (1 << 0),
	SEARCH_FLAG_FORWARDS 		= (1 << 1),
	SEARCH_FLAG_BACKWARDS 		= (1 << 2),
	SEARCH_FLAG_SHOWALL 		= (1 << 3)
} search_flags_t;

/**
 * Starts or continues an existing search.
 *
 * \param bw The browser_window to search.
 * \param context A context pointer passed to the callbacks.
 * \param flags	Flags controlling the search operation.
 * \param string The string being searched for.
 */
void browser_window_search(struct browser_window *bw, void *context, search_flags_t flags, const char *string);

/**
 * Clear up a search.
 *
 * Frees any memory used by the search.
 *
 * \param bw The browser window to clean up the search for.
 * \param context A context pointer passed to the callbacks.
 */

void browser_window_search_clear(struct browser_window *bw);

#endif
