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

#ifndef _NETSURF_RISCOS_IMAGE_H_
#define _NETSURF_RISCOS_IMAGE_H_

#include <stdbool.h>
#include "desktop/plot_style.h"
#include "oslib/osspriteop.h"

struct osspriteop_area;

typedef enum {
	IMAGE_PLOT_TINCT_ALPHA,
	IMAGE_PLOT_TINCT_OPAQUE,
	IMAGE_PLOT_OS
} image_type;

bool image_redraw(osspriteop_area *area, int x, int y, int req_width,
		int req_height, int width, int height,
		colour background_colour,
		bool repeatx, bool repeaty, bool background, image_type type);

#endif
