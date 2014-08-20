/*
 * Copyright 2011 Michael Drake <tlsa@netsurf-browser.org>
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
 * Core thumbnail handling (implementation).
 */

#include <assert.h>
#include <stdbool.h>

#include "content/content.h"
#include "content/hlcache.h"
#include "desktop/browser.h"
#include "desktop/knockout.h"
#include "utils/nsoption.h"
#include "desktop/plotters.h"
#include "desktop/thumbnail.h"
#include "utils/log.h"


/**
 * Get scale at which thumbnail will be rendered for a given content and
 * thumbnail size.
 *
 * \param  content  The content to redraw for thumbnail
 * \param  width    The thumbnail width
 * \return scale thumbnail will be rendered at
 *
 * Units for width and height are pixels.
 */
static float thumbnail_get_redraw_scale(struct hlcache_handle *content,
		int width)
{
	assert(content);

	if (content_get_width(content))
		return (float)width / (float)content_get_width(content);
	else
		return 1.0;	
}


/* exported interface, documented in thumbnail.h */
bool thumbnail_redraw(struct hlcache_handle *content,
		int width, int height, const struct redraw_context *ctx)
{
	struct redraw_context new_ctx = *ctx;
	struct rect clip;
	struct content_redraw_data data;
	float scale;
	bool plot_ok = true;

	assert(content);

	if (ctx->plot->option_knockout)
		knockout_plot_start(ctx, &new_ctx);

	/* Set clip rectangle to required thumbnail size */
	clip.x0 = 0;
	clip.y0 = 0;
	clip.x1 = width;
	clip.y1 = height;

	new_ctx.plot->clip(&clip);

	/* Plot white background */
	plot_ok &= new_ctx.plot->rectangle(clip.x0, clip.y0, clip.x1, clip.y1,
			plot_style_fill_white);

	/* Find the scale we're using */
	scale = thumbnail_get_redraw_scale(content, width);

	/* Set up content redraw data */
	data.x = 0;
	data.y = 0;
	data.width = width;
	data.height = height;

	data.background_colour = 0xFFFFFF;
	data.scale = scale;
	data.repeat_x = false;
	data.repeat_y = false;

	/* Render the content */
	plot_ok &= content_redraw(content, &data, &clip, &new_ctx);
	
	if (ctx->plot->option_knockout)
		knockout_plot_end();

	return plot_ok;
}
