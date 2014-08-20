/*
 * Copyright 2007-2008 James Bursa <bursa@users.sourceforge.net>
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
 * Content for image/svg (implementation).
 */

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <svgtiny.h>

#include "content/content_protected.h"
#include "css/css.h"
#include "desktop/plotters.h"
#include "image/svg.h"
#include "utils/messages.h"
#include "utils/utils.h"

typedef struct svg_content {
	struct content base;

	struct svgtiny_diagram *diagram;

	int current_width;
	int current_height;
} svg_content;



static nserror svg_create_svg_data(svg_content *c)
{
	union content_msg_data msg_data;

	c->diagram = svgtiny_create();
	if (c->diagram == NULL)
		goto no_memory;

	c->current_width = INT_MAX;
	c->current_height = INT_MAX;

	return NSERROR_OK;

no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
	return NSERROR_NOMEM;
}


/**
 * Create a CONTENT_SVG.
 */

static nserror svg_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	svg_content *svg;
	nserror error;

	svg = calloc(1, sizeof(svg_content));
	if (svg == NULL)
		return NSERROR_NOMEM;

	error = content__init(&svg->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(svg);
		return error;
	}

	error = svg_create_svg_data(svg);
	if (error != NSERROR_OK) {
		free(svg);
		return error;
	}

	*c = (struct content *) svg;

	return NSERROR_OK;
}



/**
 * Convert a CONTENT_SVG for display.
 */

static bool svg_convert(struct content *c)
{
	/*c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("svgTitle"),
				width, height, c->source_size);*/
	//c->size += ?;
	content_set_ready(c);
	content_set_done(c);
	/* Done: update status bar */
	content_set_status(c, "");

	return true;
}

/**
 * Reformat a CONTENT_SVG.
 */

static void svg_reformat(struct content *c, int width, int height)
{
	svg_content *svg = (svg_content *) c;
	const char *source_data;
	unsigned long source_size;

	assert(svg->diagram);

	/* Avoid reformats to same width/height as we already reformatted to */
	if (width != svg->current_width || height != svg->current_height) {
		source_data = content__get_source_data(c, &source_size);

		svgtiny_parse(svg->diagram, source_data, source_size,
				nsurl_access(content_get_url(c)),
				width, height);

		svg->current_width = width;
		svg->current_height = height;
	}

	c->width = svg->diagram->width;
	c->height = svg->diagram->height;
}


/**
 * Redraw a CONTENT_SVG.
 */

static bool svg_redraw_internal(struct content *c, int x, int y,
		int width, int height, const struct rect *clip,
		const struct redraw_context *ctx, float scale,
		colour background_colour)
{
	svg_content *svg = (svg_content *) c;
	float transform[6];
	struct svgtiny_diagram *diagram = svg->diagram;
	bool ok;
	int px, py;
	unsigned int i;
	plot_font_style_t fstyle = *plot_style_font;

	assert(diagram);

	transform[0] = (float) width / (float) c->width;
	transform[1] = 0;
	transform[2] = 0;
	transform[3] = (float) height / (float) c->height;
	transform[4] = x;
	transform[5] = y;

#define BGR(c) ((c) == svgtiny_TRANSPARENT ? NS_TRANSPARENT :		\
		((svgtiny_RED((c))) |					\
		 (svgtiny_GREEN((c)) << 8) |				\
		 (svgtiny_BLUE((c)) << 16)))

	for (i = 0; i != diagram->shape_count; i++) {
		if (diagram->shape[i].path) {
			ok = ctx->plot->path(diagram->shape[i].path,
					diagram->shape[i].path_length,
					BGR(diagram->shape[i].fill),
					diagram->shape[i].stroke_width,
					BGR(diagram->shape[i].stroke),
					transform);
			if (!ok)
				return false;

		} else if (diagram->shape[i].text) {
			px = transform[0] * diagram->shape[i].text_x +
				transform[2] * diagram->shape[i].text_y +
				transform[4];
			py = transform[1] * diagram->shape[i].text_x +
				transform[3] * diagram->shape[i].text_y +
				transform[5];

			fstyle.background = 0xffffff;
			fstyle.foreground = 0x000000;
			fstyle.size = (8 * FONT_SIZE_SCALE) * scale;

			ok = ctx->plot->text(px, py,
					diagram->shape[i].text,
					strlen(diagram->shape[i].text),
					&fstyle);
			if (!ok)
				return false;
		}
        }

#undef BGR

	return true;
}


/**
 * Redraw a CONTENT_SVG.
 */

static bool svg_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	int x = data->x;
	int y = data->y;

	if ((data->width <= 0) && (data->height <= 0)) {
		/* No point trying to plot SVG if it does not occupy a valid
		 * area */
		return true;
	}

	if ((data->repeat_x == false) && (data->repeat_y == false)) {
		/* Simple case: SVG is not tiled */
		return svg_redraw_internal(c, x, y,
				data->width, data->height,
				clip, ctx, data->scale,
				data->background_colour);
	} else {
		/* Tiled redraw required.  SVG repeats to extents of clip
		 * rectangle, in x, y or both directions */
		int x0, y0, x1, y1;

		/* Find the redraw boundaries to loop within */
		x0 = x;
		if (data->repeat_x) {
			for (; x0 > clip->x0; x0 -= data->width);
			x1 = clip->x1;
		} else {
			x1 = x + 1;
		}
		y0 = y;
		if (data->repeat_y) {
			for (; y0 > clip->y0; y0 -= data->height);
			y1 = clip->y1;
		} else {
			y1 = y + 1;
		}

		/* Repeatedly plot the SVG across the area */
		for (y = y0; y < y1; y += data->height) {
			for (x = x0; x < x1; x += data->width) {
				if (!svg_redraw_internal(c, x, y,
						data->width, data->height,
						clip, ctx, data->scale,
						data->background_colour)) {
					return false;
				}
			}
		}
	}

	return true;
}


/**
 * Destroy a CONTENT_SVG and free all resources it owns.
 */

static void svg_destroy(struct content *c)
{
	svg_content *svg = (svg_content *) c;

	if (svg->diagram != NULL)
		svgtiny_free(svg->diagram);
}


static nserror svg_clone(const struct content *old, struct content **newc)
{
	svg_content *svg;
	nserror error;

	svg = calloc(1, sizeof(svg_content));
	if (svg == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &svg->base);
	if (error != NSERROR_OK) {
		content_destroy(&svg->base);
		return error;
	}

	/* Simply replay create/convert */
	error = svg_create_svg_data(svg);
	if (error != NSERROR_OK) {
		content_destroy(&svg->base);
		return error;
	}

	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (svg_convert(&svg->base) == false) {
			content_destroy(&svg->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) svg;

	return NSERROR_OK;
}

static content_type svg_content_type(void)
{
	return CONTENT_IMAGE;
}

static const content_handler svg_content_handler = {
	.create = svg_create,
	.data_complete = svg_convert,
	.reformat = svg_reformat,
	.destroy = svg_destroy,
	.redraw = svg_redraw,
	.clone = svg_clone,
	.type = svg_content_type,
	.no_share = true
};

static const char *svg_types[] = {
	"image/svg",
	"image/svg+xml"
};


CONTENT_FACTORY_REGISTER_TYPES(svg, svg_types, svg_content_handler);


