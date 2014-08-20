/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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
 * Free text search (core)
 */

#include "content/content.h"

#include "desktop/browser_private.h"
#include "desktop/search.h"

/* exported function documented in desktop/search.h */
void browser_window_search(struct browser_window *bw, void *context,
		search_flags_t flags, const char *string)
{
	if ((bw != NULL) &&
	    (bw->current_content != NULL)) {
		content_search(bw->current_content, context, flags, string);
	}
}

/* exported function documented in desktop/search.h */
void browser_window_search_clear(struct browser_window *bw)
{
	if ((bw != NULL) &&
	    (bw->current_content != NULL)) {
		content_search_clear(bw->current_content);
	}
}
