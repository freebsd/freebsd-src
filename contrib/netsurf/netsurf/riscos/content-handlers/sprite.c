/*
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
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
 * Content for image/x-riscos-sprite (RISC OS implementation).
 *
 * No conversion is necessary: we can render RISC OS sprites directly under
 * RISC OS.
 */

#include <string.h>
#include <stdlib.h>
#include "oslib/osspriteop.h"
#include "utils/config.h"
#include "content/content_protected.h"
#include "desktop/plotters.h"
#include "riscos/gui.h"
#include "riscos/image.h"
#include "riscos/content-handlers/sprite.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#ifdef WITH_SPRITE

typedef struct sprite_content {
	struct content base;

	void *data;
} sprite_content;

static nserror sprite_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool sprite_convert(struct content *c);
static void sprite_destroy(struct content *c);
static bool sprite_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx);
static nserror sprite_clone(const struct content *old, struct content **newc);
static content_type sprite_content_type(void);

static const content_handler sprite_content_handler = {
	.create = sprite_create,
	.data_complete = sprite_convert,
	.destroy = sprite_destroy,
	.redraw = sprite_redraw,
	.clone = sprite_clone,
	.type = sprite_content_type,
	.no_share = false,
};

static const char *sprite_types[] = {
	"image/x-riscos-sprite"
};

CONTENT_FACTORY_REGISTER_TYPES(sprite, sprite_types, sprite_content_handler)

nserror sprite_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	sprite_content *sprite;
	nserror error;

	sprite = calloc(1, sizeof(sprite_content));
	if (sprite == NULL)
		return NSERROR_NOMEM;

	error = content__init(&sprite->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(sprite);
		return error;
	}

	*c = (struct content *) sprite;

	return NSERROR_OK;
}

/**
 * Convert a CONTENT_SPRITE for display.
 *
 * No conversion is necessary. We merely read the sprite dimensions.
 */

bool sprite_convert(struct content *c)
{
	sprite_content *sprite = (sprite_content *) c;
	os_error *error;
	int w, h;
	union content_msg_data msg_data;
	const char *source_data;
	unsigned long source_size;
	const void *sprite_data;
	char *title;

	source_data = content__get_source_data(c, &source_size);

	sprite_data = source_data - 4;
	osspriteop_area *area = (osspriteop_area*) sprite_data;
	sprite->data = area;

	/* check for bad data */
	if ((int)source_size + 4 != area->used) {
		msg_data.error = messages_get("BadSprite");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	error = xosspriteop_read_sprite_info(osspriteop_PTR,
			(osspriteop_area *)0x100,
			(osspriteop_id) ((char *) area + area->first),
			&w, &h, NULL, NULL);
	if (error) {
		LOG(("xosspriteop_read_sprite_info: 0x%x: %s",
				error->errnum, error->errmess));
		msg_data.error = error->errmess;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	c->width = w;
	c->height = h;

	/* set title text */
	title = messages_get_buff("SpriteTitle",
			nsurl_access_leaf(llcache_handle_get_url(c->llcache)),
			c->width, c->height);
	if (title != NULL) {
		content__set_title(c, title);
		free(title);
	}
	content_set_ready(c);
	content_set_done(c);
	/* Done: update status bar */
	content_set_status(c, "");
	return true;
}


/**
 * Destroy a CONTENT_SPRITE and free all resources it owns.
 */

void sprite_destroy(struct content *c)
{
	/* do not free c->data.sprite.data at it is simply a pointer to
	 * 4 bytes beforec->source_data. */
}


/**
 * Redraw a CONTENT_SPRITE.
 */

bool sprite_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	sprite_content *sprite = (sprite_content *) c;

	if (ctx->plot->flush && !ctx->plot->flush())
		return false;

	return image_redraw(sprite->data,
			ro_plot_origin_x + data->x * 2,
			ro_plot_origin_y - data->y * 2,
			data->width, data->height,
			c->width,
			c->height,
			data->background_colour,
			false, false, false,
			IMAGE_PLOT_OS);
}

nserror sprite_clone(const struct content *old, struct content **newc)
{
	sprite_content *sprite;
	nserror error;

	sprite = calloc(1, sizeof(sprite_content));
	if (sprite == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &sprite->base);
	if (error != NSERROR_OK) {
		content_destroy(&sprite->base);
		return error;
	}

	/* Simply rerun convert */
	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (sprite_convert(&sprite->base) == false) {
			content_destroy(&sprite->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) sprite;

	return NSERROR_OK;
}

content_type sprite_content_type(void)
{
	return CONTENT_IMAGE;
}

#endif


/**
 * Returns the bit depth of a sprite
 *
 * \param   s   sprite
 * \return  depth in bpp
 */

byte sprite_bpp(const osspriteop_header *s)
{
	/* bit 31 indicates the presence of a full alpha channel 
	 * rather than a binary mask */
	int type = ((unsigned)s->mode >> osspriteop_TYPE_SHIFT) & 15;
	byte bpp = 0;

	switch (type) {
		case osspriteop_TYPE_OLD:
		{
			bits psr;
			int val;
			if (!xos_read_mode_variable(s->mode, 
					os_MODEVAR_LOG2_BPP, &val, &psr) &&
					!(psr & _C))
				bpp = 1 << val;
		}
		break;
		case osspriteop_TYPE1BPP:  bpp = 1; break;
		case osspriteop_TYPE2BPP:  bpp = 2; break;
		case osspriteop_TYPE4BPP:  bpp = 4; break;
		case osspriteop_TYPE8BPP:  bpp = 8; break;
		case osspriteop_TYPE16BPP: bpp = 16; break;
		case osspriteop_TYPE32BPP: bpp = 32; break;
		case osspriteop_TYPE_CMYK: bpp = 32; break;
	}
	return bpp;
}
