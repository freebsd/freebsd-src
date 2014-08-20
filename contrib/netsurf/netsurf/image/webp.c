 /*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
 * Content for image/webp (libwebp implementation).
 *
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <webp/decode.h>
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "content/content_protected.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

typedef struct webp_content
{
	struct content base;

	struct bitmap *bitmap;	/**< Created NetSurf bitmap */
} webp_content;


static nserror webp_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	webp_content *webp;
	nserror error;

	webp = calloc(1, sizeof(webp_content));
	if (webp == NULL)
		return NSERROR_NOMEM;

	error = content__init(&webp->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(webp);
		return error;
	}

	*c = (struct content *) webp;

	return NSERROR_OK;
}

/**
 * Convert a CONTENT_WEBP for display.
 *
 * No conversion is necessary. We merely read the WebP dimensions.
 */

static bool webp_convert(struct content *c)
{
	webp_content *webp = (webp_content *)c;
	union content_msg_data msg_data;
	const uint8_t *data;
	unsigned char *imagebuf = NULL;
	unsigned long size;
	int width = 0, height = 0;
	char *title;
	int res = 0;
	uint8_t *res_p = NULL;

	data = (uint8_t *)content__get_source_data(c, &size);

	res = WebPGetInfo(data, size, &width, &height);
	if (res == 0) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	webp->bitmap = bitmap_create(width, height, BITMAP_NEW | BITMAP_OPAQUE);
	if (!webp->bitmap) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	imagebuf = bitmap_get_buffer(webp->bitmap);
	if (!imagebuf) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	unsigned int row_width = bitmap_get_rowstride(webp->bitmap);

	res_p = WebPDecodeRGBAInto(data, size, imagebuf,
				row_width * height, row_width);
	if (res_p == NULL) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	c->width = width;
	c->height = height;

	/* set title */
	title = messages_get_buff("WebPTitle",
			nsurl_access_leaf(llcache_handle_get_url(c->llcache)),
			c->width, c->height);
	if (title != NULL) {
		content__set_title(c, title);
		free(title);
	}

	bitmap_modified(webp->bitmap);

	content_set_ready(c);
	content_set_done(c);

	content_set_status(c, "");
	return true;
}


/**
 * Destroy a CONTENT_WEBP and free all resources it owns.
 */

static void webp_destroy(struct content *c)
{
	webp_content *webp = (webp_content *)c;

	if (webp->bitmap != NULL)
		bitmap_destroy(webp->bitmap);
}


/**
 * Redraw a CONTENT_WEBP.
 */

static bool webp_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	webp_content *webp = (webp_content *)c;
	bitmap_flags_t flags = BITMAPF_NONE;

	if (data->repeat_x)
		flags |= BITMAPF_REPEAT_X;
	if (data->repeat_y)
		flags |= BITMAPF_REPEAT_Y;

	return ctx->plot->bitmap(data->x, data->y, data->width, data->height,
			webp->bitmap, data->background_colour, flags);
}


static nserror webp_clone(const struct content *old, struct content **newc)
{
	webp_content *webp;
	nserror error;

	webp = calloc(1, sizeof(webp_content));
	if (webp == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &webp->base);
	if (error != NSERROR_OK) {
		content_destroy(&webp->base);
		return error;
	}

	/* Simply replay convert */
	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (webp_convert(&webp->base) == false) {
			content_destroy(&webp->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) webp;

	return NSERROR_OK;
}

static void *webp_get_internal(const struct content *c, void *context)
{
	webp_content *webp = (webp_content *)c;

	return webp->bitmap;
}

static content_type webp_content_type(void)
{
	return CONTENT_IMAGE;
}

static const content_handler webp_content_handler = {
	.create = webp_create,
	.data_complete = webp_convert,
	.destroy = webp_destroy,
	.redraw = webp_redraw,
	.clone = webp_clone,
	.get_internal = webp_get_internal,
	.type = webp_content_type,
	.no_share = false,
};

static const char *webp_types[] = {
	"image/webp"
};

CONTENT_FACTORY_REGISTER_TYPES(webp, webp_types, webp_content_handler);

