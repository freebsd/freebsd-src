/*
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
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
 * Content for image/gif (implementation)
 *
 * All GIFs are dynamically decompressed using the routines that gifread.c
 * provides. Whilst this allows support for progressive decoding, it is
 * not implemented here as NetSurf currently does not provide such support.
 *
 * [rjw] - Sun 4th April 2004
 */

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <libnsgif.h>

#include "utils/config.h"
#include "content/content_protected.h"
#include "content/hlcache.h"
#include "utils/nsoption.h"
#include "desktop/plotters.h"
#include "desktop/gui_factory.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include "image/image.h"
#include "image/bitmap.h"
#include "image/gif.h"

typedef struct nsgif_content {
	struct content base;

	struct gif_animation *gif; /**< GIF animation data */
	int current_frame;   /**< current frame to display [0...(max-1)] */
} nsgif_content;


/**
 * Callback for libnsgif; forwards the call to bitmap_create()
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */
static void *nsgif_bitmap_create(int width, int height)
{
	return bitmap_create(width, height, BITMAP_NEW);
}

/* The Bitmap callbacks function table;
 * necessary for interaction with nsgiflib.
 */
static gif_bitmap_callback_vt gif_bitmap_callbacks = {
	.bitmap_create = nsgif_bitmap_create,
	.bitmap_destroy = bitmap_destroy,
	.bitmap_get_buffer = bitmap_get_buffer,
	.bitmap_set_opaque = bitmap_set_opaque,
	.bitmap_test_opaque = bitmap_test_opaque,
	.bitmap_modified = bitmap_modified
};

static nserror nsgif_create_gif_data(nsgif_content *c)
{
	union content_msg_data msg_data;

	/* Initialise our data structure */
	c->gif = calloc(sizeof(gif_animation), 1);
	if (c->gif == NULL) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
		return NSERROR_NOMEM;
	}
	gif_create(c->gif, &gif_bitmap_callbacks);
	return NSERROR_OK;
}



static nserror nsgif_create(const content_handler *handler, 
		lwc_string *imime_type, const struct http_parameter *params, 
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	nsgif_content *result;
	nserror error;

	result = calloc(1, sizeof(nsgif_content));
	if (result == NULL)
		return NSERROR_NOMEM;

	error = content__init(&result->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(result);
		return error;
	}

	error = nsgif_create_gif_data(result);
	if (error != NSERROR_OK) {
		free(result);
		return error;
	}

	*c = (struct content *) result;

	return NSERROR_OK;
}

/**
 * Performs any necessary animation.
 *
 * \param p  The content to animate
*/
static void nsgif_animate(void *p)
{
	nsgif_content *gif = p;
	union content_msg_data data;
	int delay;
	int f;

	/* Advance by a frame, updating the loop count accordingly */
	gif->current_frame++;
	if (gif->current_frame == (int)gif->gif->frame_count_partial) {
		gif->current_frame = 0;

		/* A loop count of 0 has a special meaning of infinite */
		if (gif->gif->loop_count != 0) {
			gif->gif->loop_count--;
			if (gif->gif->loop_count == 0) {
				gif->current_frame =
					gif->gif->frame_count_partial - 1;
				gif->gif->loop_count = -1;
			}
		}
	}

	/* Continue animating if we should */
	if (gif->gif->loop_count >= 0) {
		delay = gif->gif->frames[gif->current_frame].frame_delay;
		if (delay < nsoption_int(minimum_gif_delay))
			delay = nsoption_int(minimum_gif_delay);
		guit->browser->schedule(delay * 10, nsgif_animate, gif);
	}

	if ((!nsoption_bool(animate_images)) ||
	    (!gif->gif->frames[gif->current_frame].display)) {
		return;
	}

	/* area within gif to redraw */
	f = gif->current_frame;
	data.redraw.x = gif->gif->frames[f].redraw_x;
	data.redraw.y = gif->gif->frames[f].redraw_y;
	data.redraw.width = gif->gif->frames[f].redraw_width;
	data.redraw.height = gif->gif->frames[f].redraw_height;

	/* redraw background (true) or plot on top (false) */
	if (gif->current_frame > 0) {
		data.redraw.full_redraw = 
				gif->gif->frames[f - 1].redraw_required;
		/* previous frame needed clearing: expand the redraw area to
		 * cover it */
		if (data.redraw.full_redraw) {
			if (data.redraw.x >
					(int)(gif->gif->frames[f - 1].redraw_x)) {
				data.redraw.width += data.redraw.x -
					gif->gif->frames[f - 1].redraw_x;
				data.redraw.x = 
					gif->gif->frames[f - 1].redraw_x;
			}
			if (data.redraw.y >
					(int)(gif->gif->frames[f - 1].redraw_y)) {
				data.redraw.height += (data.redraw.y -
					gif->gif->frames[f - 1].redraw_y);
				data.redraw.y = 
					gif->gif->frames[f - 1].redraw_y;
			}
			if ((int)(gif->gif->frames[f - 1].redraw_x +
					gif->gif->frames[f - 1].redraw_width) >
					(data.redraw.x + data.redraw.width))
				data.redraw.width =
					gif->gif->frames[f - 1].redraw_x -
					data.redraw.x +
					gif->gif->frames[f - 1].redraw_width;
			if ((int)(gif->gif->frames[f - 1].redraw_y +
					gif->gif->frames[f - 1].redraw_height) >
					(data.redraw.y + data.redraw.height))
				data.redraw.height =
					gif->gif->frames[f - 1].redraw_y -
					data.redraw.y +
					gif->gif->frames[f - 1].redraw_height;
		}
	} else {
		/* do advanced check */
		if ((data.redraw.x == 0) && (data.redraw.y == 0) &&
				(data.redraw.width == (int)(gif->gif->width)) &&
				(data.redraw.height == (int)(gif->gif->height))) {
			data.redraw.full_redraw = !gif->gif->frames[f].opaque;
		} else {
			data.redraw.full_redraw = true;
			data.redraw.x = 0;
			data.redraw.y = 0;
			data.redraw.width = gif->gif->width;
			data.redraw.height = gif->gif->height;
		}
	}

	/* other data */
	data.redraw.object = (struct content *) gif;
	data.redraw.object_x = 0;
	data.redraw.object_y = 0;
	data.redraw.object_width = gif->base.width;
	data.redraw.object_height = gif->base.height;

	content_broadcast(&gif->base, CONTENT_MSG_REDRAW, data);
}

static bool nsgif_convert(struct content *c)
{
	nsgif_content *gif = (nsgif_content *) c;
	int res;
	union content_msg_data msg_data;
	const char *data;
	unsigned long size;
	char *title;

	/* Get the animation */
	data = content__get_source_data(c, &size);

	/* Initialise the GIF */
	do {
		res = gif_initialise(gif->gif, size, (unsigned char *) data);
		if (res != GIF_OK && res != GIF_WORKING && 
				res != GIF_INSUFFICIENT_FRAME_DATA) {
			switch (res) {
			case GIF_FRAME_DATA_ERROR:
			case GIF_INSUFFICIENT_DATA:
			case GIF_DATA_ERROR:
				msg_data.error = messages_get("BadGIF");
				break;
			case GIF_INSUFFICIENT_MEMORY:
				msg_data.error = messages_get("NoMemory");
				break;
			}
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}
	} while (res != GIF_OK && res != GIF_INSUFFICIENT_FRAME_DATA);

	/* Abort on bad GIFs */
	if ((gif->gif->frame_count_partial == 0) || (gif->gif->width == 0) ||
			(gif->gif->height == 0)) {
		msg_data.error = messages_get("BadGIF");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/* Store our content width, height and calculate size */
	c->width = gif->gif->width;
	c->height = gif->gif->height;
	c->size += (gif->gif->width * gif->gif->height * 4) + 16 + 44;

	/* set title text */
	title = messages_get_buff("GIFTitle",
			nsurl_access_leaf(llcache_handle_get_url(c->llcache)),
			c->width, c->height);
	if (title != NULL) {
		content__set_title(c, title);
		free(title);
	}

	/* Schedule the animation if we have one */
	gif->current_frame = 0;
	if (gif->gif->frame_count_partial > 1)
		guit->browser->schedule(gif->gif->frames[0].frame_delay * 10,
					nsgif_animate,
					c);

	/* Exit as a success */
	content_set_ready(c);
	content_set_done(c);

	/* Done: update status bar */
	content_set_status(c, "");
	return true;
}


/**
 * Updates the GIF bitmap to display the current frame
 *
 * \param c  the content to update
 */
static gif_result nsgif_get_frame(nsgif_content *gif)
{
	int previous_frame, current_frame, frame;
	gif_result res = GIF_OK;

	current_frame = gif->current_frame;
	if (!nsoption_bool(animate_images)) {
		current_frame = 0;
	}

	if (current_frame < gif->gif->decoded_frame)
		previous_frame = 0;
	else
		previous_frame = gif->gif->decoded_frame + 1;

	for (frame = previous_frame; frame <= current_frame; frame++) {
		res = gif_decode_frame(gif->gif, frame);
	}

	return res;
}

static bool nsgif_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	nsgif_content *gif = (nsgif_content *) c;

	if (gif->current_frame != gif->gif->decoded_frame) {
		if (nsgif_get_frame(gif) != GIF_OK) {
			return false;
		}
	}

	return image_bitmap_plot(gif->gif->frame_image, data, clip, ctx);
}


static void nsgif_destroy(struct content *c)
{
	nsgif_content *gif = (nsgif_content *) c;

	/* Free all the associated memory buffers */
	guit->browser->schedule(-1, nsgif_animate, c);
	gif_finalise(gif->gif);
	free(gif->gif);
}


static nserror nsgif_clone(const struct content *old, struct content **newc)
{
	nsgif_content *gif;
	nserror error;

	gif = calloc(1, sizeof(nsgif_content));
	if (gif == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &gif->base);
	if (error != NSERROR_OK) {
		content_destroy(&gif->base);
		return error;
	}

	/* Simply replay creation and conversion of content */
	error = nsgif_create_gif_data(gif);
	if (error != NSERROR_OK) {
		content_destroy(&gif->base);
		return error;
	}

	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (nsgif_convert(&gif->base) == false) {
			content_destroy(&gif->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) gif;

	return NSERROR_OK;
}

static void *nsgif_get_internal(const struct content *c, void *context)
{
	nsgif_content *gif = (nsgif_content *) c;

	if (gif->current_frame != gif->gif->decoded_frame) {
		if (nsgif_get_frame(gif) != GIF_OK)
			return NULL;
	}

	return gif->gif->frame_image;
}

static content_type nsgif_content_type(void)
{
	return CONTENT_IMAGE;
}

static const content_handler nsgif_content_handler = {
	.create = nsgif_create,
	.data_complete = nsgif_convert,
	.destroy = nsgif_destroy,
	.redraw = nsgif_redraw,
	.clone = nsgif_clone,
	.get_internal = nsgif_get_internal,
	.type = nsgif_content_type,
	.no_share = false,
};

static const char *nsgif_types[] = {
	"image/gif"
};

CONTENT_FACTORY_REGISTER_TYPES(nsgif, nsgif_types, nsgif_content_handler);
