/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2006 Adrian Lees <adrianl@users.sourceforge.net>
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
 * Content for text/plain (interface).
 */

#ifndef _NETSURF_RENDER_TEXTPLAIN_H_
#define _NETSURF_RENDER_TEXTPLAIN_H_

#include <stddef.h>
#include "desktop/mouse.h"

struct content;
struct hlcache_handle;
struct http_parameter;
struct rect;

nserror textplain_init(void);

/* access to lines for text selection and searching */
unsigned long textplain_line_count(struct content *c);
size_t textplain_size(struct content *c);

size_t textplain_offset_from_coords(struct content *c, int x, int y, int dir);
void textplain_coords_from_range(struct content *c,
		unsigned start, unsigned end, struct rect *r);
char *textplain_get_line(struct content *c, unsigned lineno,
		size_t *poffset, size_t *plen);
int textplain_find_line(struct content *c, unsigned offset);
char *textplain_get_raw_data(struct content *c,
		unsigned start, unsigned end, size_t *plen);
struct browser_window *textplain_get_browser_window(struct content *c);

#endif
