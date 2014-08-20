/*
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
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
 * Content handler for image/svg using librsvg (implementation).
 *
 * SVG files are rendered to a NetSurf bitmap by creating a Cairo rendering
 * surface (content_rsvg_data.cs) over the bitmap's data, creating a Cairo
 * drawing context using that surface, and then passing that drawing context
 * to librsvg which then uses Cairo calls to plot the graphic to the bitmap.
 * We store this in content->bitmap, and then use the usual bitmap plotter
 * function to render it for redraw requests.
 */

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>

#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#include "content/content_protected.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/messages.h"

#include "image/rsvg.h"

typedef struct rsvg_content {
	struct content base;

	RsvgHandle *rsvgh;	/**< Context handle for RSVG renderer */
	cairo_surface_t *cs;	/**< The surface built inside a nsbitmap */
	cairo_t *ct;		/**< Cairo drawing context */
	struct bitmap *bitmap;	/**< Created NetSurf bitmap */
} rsvg_content;

static nserror rsvg_create_svg_data(rsvg_content *c)
{
	union content_msg_data msg_data;

	c->rsvgh = NULL;
	c->cs = NULL;
	c->ct = NULL;
	c->bitmap = NULL;

	if ((c->rsvgh = rsvg_handle_new()) == NULL) {
		LOG(("rsvg_handle_new() returned NULL."));
		msg_data.error = messages_get("NoMemory");
		content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}


static nserror rsvg_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	rsvg_content *svg;
	nserror error;

	svg = calloc(1, sizeof(rsvg_content));
	if (svg == NULL)
		return NSERROR_NOMEM;

	error = content__init(&svg->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(svg);
		return error;
	}

	error = rsvg_create_svg_data(svg);
	if (error != NSERROR_OK) {
		free(svg);
		return error;
	}

	*c = (struct content *) svg;

	return NSERROR_OK;
}


static bool rsvg_process_data(struct content *c, const char *data,
			unsigned int size)
{
	rsvg_content *d = (rsvg_content *) c;
	union content_msg_data msg_data;
	GError *err = NULL;

	if (rsvg_handle_write(d->rsvgh, (const guchar *)data, (gsize)size,
				&err) == FALSE) {
		LOG(("rsvg_handle_write returned an error: %s", err->message));
		msg_data.error = err->message;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	return true;
}

/** Convert Cairo's ARGB output to NetSurf's favoured ABGR format.  It converts
 * the data in-place. 
 *
 * \param pixels	Pixel data, in the form of ARGB.  This will
 *			be overwritten with new data in the form of ABGR.
 * \param width		Width of the bitmap
 * \param height	Height of the bitmap
 * \param rowstride	Number of bytes to skip after each row (this
 *			implementation requires this to be a multiple of 4.)
 */
static inline void rsvg_argb_to_abgr(uint8_t *pixels, 
		int width, int height, size_t rowstride)
{
	uint8_t *p = pixels;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			/* Swap R and B */
			const uint8_t r = p[x+3];

			p[x+3] = p[x];

			p[x] = r;
		}

		p += rowstride;
	}
}

static bool rsvg_convert(struct content *c)
{
	rsvg_content *d = (rsvg_content *) c;
	union content_msg_data msg_data;
	RsvgDimensionData rsvgsize;
	GError *err = NULL;

	if (rsvg_handle_close(d->rsvgh, &err) == FALSE) {
		LOG(("rsvg_handle_close returned an error: %s", err->message));
		msg_data.error = err->message;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	assert(err == NULL);

	/* we should now be able to query librsvg for the natural size of the
	 * graphic, so we can create our bitmap.
	 */

	rsvg_handle_get_dimensions(d->rsvgh, &rsvgsize);
	c->width = rsvgsize.width;
	c->height = rsvgsize.height;

	if ((d->bitmap = bitmap_create(c->width, c->height,
			BITMAP_NEW)) == NULL) {
		LOG(("Failed to create bitmap for rsvg render."));
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	if ((d->cs = cairo_image_surface_create_for_data(
			(unsigned char *)bitmap_get_buffer(d->bitmap),
			CAIRO_FORMAT_ARGB32,
			c->width, c->height,
			bitmap_get_rowstride(d->bitmap))) == NULL) {
		LOG(("Failed to create Cairo image surface for rsvg render."));
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	if ((d->ct = cairo_create(d->cs)) == NULL) {
		LOG(("Failed to create Cairo drawing context for rsvg render."));
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	rsvg_handle_render_cairo(d->rsvgh, d->ct);
	rsvg_argb_to_abgr(bitmap_get_buffer(d->bitmap),
				c->width, c->height,
				bitmap_get_rowstride(d->bitmap));

	bitmap_modified(d->bitmap);
	content_set_ready(c);
	content_set_done(c);
	/* Done: update status bar */
	content_set_status(c, "");

	return true;
}

static bool rsvg_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	rsvg_content *rsvgcontent = (rsvg_content *) c;
	bitmap_flags_t flags = BITMAPF_NONE;

	assert(rsvgcontent->bitmap != NULL);

	if (data->repeat_x)
		flags |= BITMAPF_REPEAT_X;
	if (data->repeat_y)
		flags |= BITMAPF_REPEAT_Y;

	return ctx->plot->bitmap(data->x, data->y, data->width, data->height, 
			rsvgcontent->bitmap, data->background_colour, flags);
}

static void rsvg_destroy(struct content *c)
{
	rsvg_content *d = (rsvg_content *) c;

	if (d->bitmap != NULL) bitmap_destroy(d->bitmap);
	if (d->rsvgh != NULL) g_object_unref(d->rsvgh);
	if (d->ct != NULL) cairo_destroy(d->ct);
	if (d->cs != NULL) cairo_surface_destroy(d->cs);

	return;
}

static nserror rsvg_clone(const struct content *old, struct content **newc)
{
	rsvg_content *svg;
	nserror error;
	const char *data;
	unsigned long size;

	svg = calloc(1, sizeof(rsvg_content));
	if (svg == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &svg->base);
	if (error != NSERROR_OK) {
		content_destroy(&svg->base);
		return error;
	}

	/* Simply replay create/process/convert */
	error = rsvg_create_svg_data(svg);
	if (error != NSERROR_OK) {
		content_destroy(&svg->base);
		return error;
	}

	data = content__get_source_data(&svg->base, &size);
	if (size > 0) {
		if (rsvg_process_data(&svg->base, data, size) == false) {
			content_destroy(&svg->base);
			return NSERROR_NOMEM;
		}
	}

	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (rsvg_convert(&svg->base) == false) {
			content_destroy(&svg->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) svg;

	return NSERROR_OK;
}

static void *rsvg_get_internal(const struct content *c, void *context)
{
	rsvg_content *d = (rsvg_content *) c;

	return d->bitmap;
}

static content_type rsvg_content_type(void)
{
	return CONTENT_IMAGE;
}

static const content_handler rsvg_content_handler = {
	.create = rsvg_create,
	.process_data = rsvg_process_data,
	.data_complete = rsvg_convert,
	.destroy = rsvg_destroy,
	.redraw = rsvg_redraw,
	.clone = rsvg_clone,
	.get_internal = rsvg_get_internal,
	.type = rsvg_content_type,
	.no_share = false,
};

static const char *rsvg_types[] = {
	"image/svg",
	"image/svg+xml"
};

CONTENT_FACTORY_REGISTER_TYPES(nsrsvg, rsvg_types, rsvg_content_handler);

