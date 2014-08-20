/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
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

#include <string.h>

#include "utils/log.h"

#include "gtk/gdk.h"

static void
convert_alpha(guchar  *dest_data,
               int      dest_stride,
               guchar  *src_data,
               int      src_stride,
               int      width,
               int      height)
{
	int x, y;

	for (y = 0; y < height; y++) {
		guint32 *src = (guint32 *) src_data;

		for (x = 0; x < width; x++) {
			guint alpha = src[x] >> 24;

			if (alpha == 0)	{
				dest_data[x * 4 + 0] = 0;
				dest_data[x * 4 + 1] = 0;
				dest_data[x * 4 + 2] = 0;
			} else {
				dest_data[x * 4 + 0] = (((src[x] & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
				dest_data[x * 4 + 1] = (((src[x] & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
				dest_data[x * 4 + 2] = (((src[x] & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
			}
			dest_data[x * 4 + 3] = alpha;
		}

		src_data += src_stride;
		dest_data += dest_stride;
	}
}


GdkPixbuf *
nsgdk_pixbuf_get_from_surface(cairo_surface_t *surface, int scwidth, int scheight)
{
	int width, height; /* source width and height */
	cairo_surface_t *scsurface; /* scaled surface */
	cairo_t *cr; /* cairo context for scaled surface */
	GdkPixbuf *pixbuf; /* The result pixel buffer */

	/* create pixmap */
	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, scwidth, scheight);
	if (pixbuf == NULL) {
		return NULL;
	}

	memset(gdk_pixbuf_get_pixels(pixbuf),
	       0xff, 
	       gdk_pixbuf_get_rowstride(pixbuf) * scheight);

	/* scale cairo surface into new surface the target size */
	cairo_surface_flush(surface); /* ensure source surface is ready */

	/* get source surface dimensions */
	width = cairo_image_surface_get_width(surface);
	height = cairo_image_surface_get_height(surface);

	/* scaled surface always has an alpha chanel for ease */
	scsurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, scwidth, scheight);
	if (cairo_surface_status(scsurface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(scsurface);
		g_object_unref(pixbuf);
		LOG(("Surface creation failed"));
		return NULL;
	}

	cr = cairo_create(scsurface);

	/* Scale *before* setting the source surface */
	cairo_scale(cr, (double)scwidth / width, (double)scheight / height);
	cairo_set_source_surface(cr, surface, 0, 0);

	/* To avoid getting the edge pixels blended with 0
	 * alpha, which would occur with the default
	 * EXTEND_NONE. Use EXTEND_PAD for 1.2 or newer
	 */
	cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REFLECT); 

	/* Replace the destination with the source instead of overlaying */
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

	/* Do the actual drawing */
	cairo_paint(cr);
   
	cairo_destroy(cr);

	/* copy data from surface into pixmap */
	convert_alpha(gdk_pixbuf_get_pixels(pixbuf),
		      gdk_pixbuf_get_rowstride(pixbuf),
		      cairo_image_surface_get_data(scsurface),
		      cairo_image_surface_get_stride(scsurface),
		      scwidth, scheight);

	cairo_surface_destroy(scsurface);

	return pixbuf;
}

