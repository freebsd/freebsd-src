/*
 * Copyright 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
 * DataTypes sound handler (implementation)
*/

#ifdef WITH_AMIGA_DATATYPES
#include "amiga/filetype.h"
#include "amiga/datatypes.h"
#include "content/content_protected.h"
#include "desktop/plotters.h"
#include "render/box.h"
#include "utils/log.h"
#include "utils/messages.h"

#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <datatypes/soundclass.h>
#include <intuition/classusr.h>

typedef struct amiga_dt_sound_content {
	struct content base;

	Object *dto;
	bool immediate;
} amiga_dt_sound_content;

static nserror amiga_dt_sound_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool amiga_dt_sound_convert(struct content *c);
static void amiga_dt_sound_destroy(struct content *c);
static bool amiga_dt_sound_redraw(struct content *c,
		struct content_redraw_data *data, const struct rect *clip,
		const struct redraw_context *ctx);
static void amiga_dt_sound_open(struct content *c, struct browser_window *bw,
		struct content *page, struct object_params *params);
static nserror amiga_dt_sound_clone(const struct content *old, struct content **newc);
static content_type amiga_dt_sound_content_type(void);

static const content_handler amiga_dt_sound_content_handler = {
	.create = amiga_dt_sound_create,
	.data_complete = amiga_dt_sound_convert,
	.destroy = amiga_dt_sound_destroy,
	.redraw = amiga_dt_sound_redraw,
	.open = amiga_dt_sound_open,
	.clone = amiga_dt_sound_clone,
	.type = amiga_dt_sound_content_type,
	.no_share = false,
};


void amiga_dt_sound_play(Object *dto)
{
	LOG(("Playing..."));
	IDoMethod(dto, DTM_TRIGGER, NULL, STM_PLAY, NULL);
}


nserror amiga_dt_sound_init(void)
{
	struct DataType *dt, *prevdt = NULL;
	lwc_string *type;
	lwc_error lerror;
	nserror error;
	BPTR fh = 0;
	struct Node *node = NULL;

	while((dt = ObtainDataType(DTST_RAM, NULL,
			DTA_DataType, prevdt,
			DTA_GroupID, GID_SOUND,
			TAG_DONE)) != NULL)
	{
		ReleaseDataType(prevdt);
		prevdt = dt;

		do {
			node = ami_mime_from_datatype(dt, &type, node);

			if(node)
			{
				error = content_factory_register_handler(
					lwc_string_data(type), 
					&amiga_dt_sound_content_handler);

				if (error != NSERROR_OK)
					return error;
			}

		}while (node != NULL);

	}

	ReleaseDataType(prevdt);

	return NSERROR_OK;
}

nserror amiga_dt_sound_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	amiga_dt_sound_content *plugin;
	nserror error;

	LOG(("amiga_dt_sound_create"));

	plugin = calloc(1, sizeof(amiga_dt_sound_content));
	if (plugin == NULL)
		return NSERROR_NOMEM;

	error = content__init(&plugin->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(plugin);
		return error;
	}

	*c = (struct content *) plugin;

	return NSERROR_OK;
}

bool amiga_dt_sound_convert(struct content *c)
{
	LOG(("amiga_dt_sound_convert"));

	amiga_dt_sound_content *plugin = (amiga_dt_sound_content *) c;
	int width = 50, height = 50;
	const uint8 *data;
	ULONG size;

	data = (uint8 *)content__get_source_data(c, &size);

	plugin->dto = NewDTObject(NULL,
					DTA_SourceType, DTST_MEMORY,
					DTA_SourceAddress, data,
					DTA_SourceSize, size,
					DTA_GroupID, GID_SOUND,
					TAG_DONE);

	if(plugin->dto == NULL) return false;

	c->width = width;
	c->height = height;

	if(plugin->immediate == true) amiga_dt_sound_play(plugin->dto);

	content_set_ready(c);
	content_set_done(c);

	content_set_status(c, "");
	return true;
}

void amiga_dt_sound_destroy(struct content *c)
{
	amiga_dt_sound_content *plugin = (amiga_dt_sound_content *) c;

	LOG(("amiga_dt_sound_destroy"));

	DisposeDTObject(plugin->dto);

	return;
}

bool amiga_dt_sound_redraw(struct content *c,
		struct content_redraw_data *data, const struct rect *clip,
		const struct redraw_context *ctx)
{
	plot_style_t pstyle = {
		.fill_type = PLOT_OP_TYPE_SOLID,
		.fill_colour = 0xffffff,
		.stroke_colour = 0x000000,
		.stroke_width = 1,
	};

	LOG(("amiga_dt_sound_redraw"));

	/* this should be some sort of play/stop control */

	ctx->plot->rectangle(data->x, data->y, data->x + data->width,
			data->y + data->height, &pstyle);

	return ctx->plot->text(data->x, data->y+20,
			lwc_string_data(content__get_mime_type(c)),
			lwc_string_length(content__get_mime_type(c)),
			plot_style_font);

}


void amiga_dt_sound_open(struct content *c, struct browser_window *bw,
	struct content *page, struct object_params *params)
{
	amiga_dt_sound_content *plugin = (amiga_dt_sound_content *) c;
	struct object_param *param;

	LOG(("amiga_dt_sound_open"));

	plugin->immediate = false;

	if(params && (param = params->params))
	{
		do
		{
			LOG(("%s = %s", param->name, param->value));
			if((strcmp(param->name, "autoplay") == 0) &&
				(strcmp(param->value, "true") == 0)) plugin->immediate = true;
			if((strcmp(param->name, "autoStart") == 0) &&
				(strcmp(param->value, "1") == 0)) plugin->immediate = true;
			param = param->next;
		} while(param != NULL);
	}

	if(plugin->dto && (plugin->immediate == true))
		amiga_dt_sound_play(plugin->dto);

	return;
}


nserror amiga_dt_sound_clone(const struct content *old, struct content **newc)
{
	amiga_dt_sound_content *plugin;
	nserror error;

	LOG(("amiga_dt_sound_clone"));

	plugin = calloc(1, sizeof(amiga_dt_sound_content));
	if (plugin == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &plugin->base);
	if (error != NSERROR_OK) {
		content_destroy(&plugin->base);
		return error;
	}

	/* We "clone" the old content by replaying conversion */
	if (old->status == CONTENT_STATUS_READY || 
			old->status == CONTENT_STATUS_DONE) {
		if (amiga_dt_sound_convert(&plugin->base) == false) {
			content_destroy(&plugin->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) plugin;

	return NSERROR_OK;
}

content_type amiga_dt_sound_content_type(void)
{
	return CONTENT_PLUGIN;
}

#endif
