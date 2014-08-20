/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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

#ifndef _NETSURF_RENDER_IMAGEMAP_H_
#define _NETSURF_RENDER_IMAGEMAP_H_

#include <dom/dom.h>

#include "utils/nsurl.h"

struct html_content;
struct hlcache_handle;

void imagemap_destroy(struct html_content *c);
void imagemap_dump(struct html_content *c);
nserror imagemap_extract(struct html_content *c);

nsurl *imagemap_get(struct html_content *c, const char *key,
		unsigned long x, unsigned long y,
		unsigned long click_x, unsigned long click_y,
		const char **target);

#endif
