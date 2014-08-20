/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
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
  * Text export of HTML (interface).
  */

#ifndef _NETSURF_DESKTOP_SAVE_TEXT_H_
#define _NETSURF_DESKTOP_SAVE_TEXT_H_

struct box;
struct hlcache_handle;

/* text currently being saved */
struct save_text_state {
	char *block;
	size_t length;
	size_t alloc;
};

typedef enum {
	WHITESPACE_NONE,
	WHITESPACE_TAB,
	WHITESPACE_ONE_NEW_LINE,
	WHITESPACE_TWO_NEW_LINES
} save_text_whitespace;

void save_as_text(struct hlcache_handle *c, char *path);
void save_text_solve_whitespace(struct box *box, bool *first,
		save_text_whitespace *before, const char **whitespace_text,
		size_t *whitespace_length);

#endif
