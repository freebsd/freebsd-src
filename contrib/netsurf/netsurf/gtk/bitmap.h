/*
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#ifndef NS_GTK_BITMAP_H
#define NS_GTK_BITMAP_H

#include <cairo.h>
#include "image/bitmap.h"

struct bitmap {
	cairo_surface_t *surface; /* original cairo surface */
	cairo_surface_t *scsurface; /* scaled surface */
	bool converted; /** set if the surface data has been converted */
};

#endif /* NS_GTK_BITMAP_H */
