/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <stdbool.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>

#include "utils/log.h"
#include "desktop/thumbnail.h"
#include "content/urldb.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/framebuffer.h"

bool
thumbnail_create(struct hlcache_handle *content,
		 struct bitmap *bitmap,
		 nsurl *url)
{
	nsfb_t *tbm = (nsfb_t *)bitmap; /* target bitmap */
	nsfb_t *bm; /* temporary bitmap */
	nsfb_t *current; /* current main fb */
	int width, height; /* target bitmap width height */
	int cwidth, cheight;/* content width /height */
	nsfb_bbox_t loc;

	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &fb_plotters
	};


	nsfb_get_geometry(tbm, &width, &height, NULL);

	LOG(("width %d, height %d", width, height));

	/* Calculate size of buffer to render the content into */
	/* We get the width from the content width, unless it exceeds 1024,
	 * in which case we use 1024. This means we never create excessively
	 * large render buffers for huge contents, which would eat memory and
	 * cripple performance. */
	cwidth = min(content_get_width(content), 1024);
	/* The height is set in proportion with the width, according to the
	 * aspect ratio of the required thumbnail. */
	cheight = ((cwidth * height) + (width / 2)) / width;

	/* create temporary surface */
	bm = nsfb_new(NSFB_SURFACE_RAM);
	if (bm == NULL) {
		return false;
	}

	nsfb_set_geometry(bm, cwidth, cheight, NSFB_FMT_XBGR8888);

	if (nsfb_init(bm) == -1) {
		nsfb_free(bm);
		return false;
	}

	current = framebuffer_set_surface(bm);

	/* render the content into temporary surface */
	thumbnail_redraw(content, cwidth, cheight, &ctx);

	framebuffer_set_surface(current);

	loc.x0 = 0;
	loc.y0 = 0;
	loc.x1 = width;
	loc.y1 = height;

	nsfb_plot_copy(bm, NULL, tbm, &loc);

	nsfb_free(bm);

	/* register the thumbnail with the URL */
	if (url != NULL)
		urldb_set_thumbnail(url, bitmap);

	return true;
}
