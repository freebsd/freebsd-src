/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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
 * Page thumbnail creation (implementation).
 *
 * Thumbnails are created by setting the current drawing contexts to the
 * bitmap (a gdk pixbuf) we are passed, and plotting the page at a small
 * scale.
 */

#include <assert.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/plotters.h"
#include "desktop/browser.h"
#include "desktop/thumbnail.h"
#include "gtk/scaffolding.h"
#include "gtk/plotters.h"
#include "gtk/bitmap.h"
#include "image/bitmap.h"
#include "render/font.h"
#include "utils/log.h"
#include "utils/utils.h"

/**
 * Create a thumbnail of a page.
 *
 * \param  content  content structure to thumbnail
 * \param  bitmap   the bitmap to draw to
 * \param  url      the URL the thumnail belongs to, or NULL
 */
bool thumbnail_create(hlcache_handle *content, struct bitmap *bitmap,
		nsurl *url)
{
	cairo_surface_t *dsurface = bitmap->surface;
	cairo_surface_t *surface;
	cairo_t *old_cr;
	gint dwidth, dheight;
	int cwidth, cheight;
	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &nsgtk_plotters
	};

	assert(content);
	assert(bitmap);

	dwidth = cairo_image_surface_get_width(dsurface);
	dheight = cairo_image_surface_get_height(dsurface);

	/* Calculate size of buffer to render the content into */
	/* We get the width from the content width, unless it exceeds 1024,
	 * in which case we use 1024. This means we never create excessively
	 * large render buffers for huge contents, which would eat memory and
	 * cripple performance. */
	cwidth = min(content_get_width(content), 1024);

	/* The height is set in proportion with the width, according to the
	 * aspect ratio of the required thumbnail. */
	cheight = ((cwidth * dheight) + (dwidth / 2)) / dwidth;

	/*  Create surface to render into */
	surface = cairo_surface_create_similar(dsurface, CAIRO_CONTENT_COLOR_ALPHA, cwidth, cheight);

	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return false;
	}

	old_cr = current_cr;
	current_cr = cairo_create(surface);

	/* render the content */
	thumbnail_redraw(content, cwidth, cheight, &ctx);

	cairo_destroy(current_cr);
	current_cr = old_cr;

	cairo_t *cr = cairo_create(dsurface);

	/* Scale *before* setting the source surface (1) */
	cairo_scale (cr, (double)dwidth / cwidth, (double)dheight / cheight);
	cairo_set_source_surface (cr, surface, 0, 0);

	/* To avoid getting the edge pixels blended with 0 alpha,
	 * which would occur with the default EXTEND_NONE. Use
	 * EXTEND_PAD for 1.2 or newer (2)
	 */
	cairo_pattern_set_extend (cairo_get_source(cr), CAIRO_EXTEND_REFLECT); 

	/* Replace the destination with the source instead of overlaying */
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

	/* Do the actual drawing */
	cairo_paint(cr);
   
	cairo_destroy(cr);

	cairo_surface_destroy(surface);

	/* register the thumbnail with the URL */
	if (url)
		urldb_set_thumbnail(url, bitmap);

	return true;
}

