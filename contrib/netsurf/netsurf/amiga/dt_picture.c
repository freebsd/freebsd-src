/*
 * Copyright 2011 - 2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
 * DataTypes picture handler (implementation)
*/

#ifdef WITH_AMIGA_DATATYPES
#include "amiga/filetype.h"
#include "amiga/datatypes.h"
#include "content/content_protected.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "image/image_cache.h"
#include "utils/log.h"
#include "utils/messages.h"

#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <datatypes/pictureclass.h>
#include <intuition/classusr.h>

static nserror amiga_dt_picture_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool amiga_dt_picture_convert(struct content *c);
static nserror amiga_dt_picture_clone(const struct content *old, struct content **newc);
static void amiga_dt_picture_destroy(struct content *c);

static const content_handler amiga_dt_picture_content_handler = {
	.create = amiga_dt_picture_create,
	.data_complete = amiga_dt_picture_convert,
	.destroy = amiga_dt_picture_destroy,
	.redraw = image_cache_redraw,
	.clone = amiga_dt_picture_clone,
	.get_internal = image_cache_get_internal,
	.type = image_cache_content_type,
	.no_share = false,
};

struct amiga_dt_picture_content {
	struct content c;
	Object *dto;
};

nserror amiga_dt_picture_init(void)
{
	struct DataType *dt, *prevdt = NULL;
	lwc_string *type;
	lwc_error lerror;
	nserror error;
	BPTR fh = 0;
	struct Node *node = NULL;

	while((dt = ObtainDataType(DTST_RAM, NULL,
			DTA_DataType, prevdt,
			DTA_GroupID, GID_PICTURE, // we only support images for now
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
					&amiga_dt_picture_content_handler);

				if (error != NSERROR_OK)
					return error;
			}

		}while (node != NULL);

	}

	ReleaseDataType(prevdt);

	return NSERROR_OK;
}

nserror amiga_dt_picture_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	struct amiga_dt_picture_content *adt;
	nserror error;

	adt = calloc(1, sizeof(struct amiga_dt_picture_content));
	if (adt == NULL)
		return NSERROR_NOMEM;

	error = content__init((struct content *)adt, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(adt);
		return error;
	}

	*c = (struct content *)adt;

	return NSERROR_OK;
}

Object *amiga_dt_picture_newdtobject(struct amiga_dt_picture_content *adt)
{
	const uint8 *data;
	ULONG size;

	if(adt->dto == NULL) {
		data = (uint8 *)content__get_source_data((struct content *)adt, &size);

		adt->dto = NewDTObject(NULL,
					DTA_SourceType, DTST_MEMORY,
					DTA_SourceAddress, data,
					DTA_SourceSize, size,
					DTA_GroupID, GID_PICTURE,
					PDTA_DestMode, PMODE_V43,
					PDTA_PromoteMask, TRUE,
					TAG_DONE);
	}

	return adt->dto;
}

char *amiga_dt_picture_datatype(struct content *c)
{
	const uint8 *data;
	ULONG size;
	struct DataType *dt;
	char *filetype = NULL;
	
	data = (uint8 *)content__get_source_data(c, &size);

	if(dt = ObtainDataType(DTST_MEMORY, NULL,
					DTA_SourceAddress, data,
					DTA_SourceSize, size,
					DTA_GroupID, GID_PICTURE,
					TAG_DONE)) {
		filetype = strdup(dt->dtn_Header->dth_Name);
		ReleaseDataType(dt);
	}
	
	if(filetype == NULL) filetype = strdup("DataTypes");
	return filetype;
}

static struct bitmap *amiga_dt_picture_cache_convert(struct content *c)
{
	LOG(("amiga_dt_picture_cache_convert"));

	union content_msg_data msg_data;
	UBYTE *bm_buffer;
	Object *dto;
	struct bitmap *bitmap;
#ifdef __amigaos4__
	int bm_format = PBPAFMT_RGBA;
#else
	int bm_format = PBPAFMT_ARGB;
#endif
	struct amiga_dt_picture_content *adt = (struct amiga_dt_picture_content *)c;

	if(dto = amiga_dt_picture_newdtobject(adt))
	{
		bitmap = bitmap_create(c->width, c->height, BITMAP_NEW);
		if (!bitmap) {
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return NULL;
		}

		bm_buffer = bitmap_get_buffer(bitmap);

		IDoMethod(dto, PDTM_READPIXELARRAY,
			bm_buffer, bm_format, bitmap_get_rowstride(bitmap),
			0, 0, c->width, c->height);
#ifndef __amigaos4__
		ami_bitmap_argb_to_rgba(bitmap);
#endif
		bitmap_set_opaque(bitmap, bitmap_test_opaque(bitmap));
		
		DisposeDTObject(dto);
		adt->dto = NULL;
	}
	else return NULL;

	return bitmap;
}

bool amiga_dt_picture_convert(struct content *c)
{
	LOG(("amiga_dt_picture_convert"));

	int width, height;
	char *title;
	Object *dto;
	struct BitMapHeader *bmh;
	char *filetype;

	if(dto = amiga_dt_picture_newdtobject((struct amiga_dt_picture_content *)c))
	{
		if(GetDTAttrs(dto, PDTA_BitMapHeader, &bmh, TAG_DONE))
		{
			width = (int)bmh->bmh_Width;
			height = (int)bmh->bmh_Height;
		}
		else return false;
	}
	else return false;

	c->width = width;
	c->height = height;
	c->size = width * height * 4;

	/* set title text */
	if(filetype = amiga_dt_picture_datatype(c)) {
		title = messages_get_buff("DataTypesTitle",
			nsurl_access_leaf(llcache_handle_get_url(c->llcache)),
			filetype, c->width, c->height);
		if (title != NULL) {
			content__set_title(c, title);
			free(title);
		}
		free(filetype);
	}
	
	image_cache_add(c, NULL, amiga_dt_picture_cache_convert);

	content_set_ready(c);
	content_set_done(c);
	content_set_status(c, "");
	return true;
}

nserror amiga_dt_picture_clone(const struct content *old, struct content **newc)
{
	struct content *adt;
	nserror error;

	LOG(("amiga_dt_picture_clone"));

	adt = calloc(1, sizeof(struct content));
	if (adt == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, adt);
	if (error != NSERROR_OK) {
		content_destroy(adt);
		return error;
	}

	/* We "clone" the old content by replaying conversion */
	if ((old->status == CONTENT_STATUS_READY) || 
	    (old->status == CONTENT_STATUS_DONE)) {
		if (amiga_dt_picture_convert(adt) == false) {
			content_destroy(adt);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = adt;

	return NSERROR_OK;
}

static void amiga_dt_picture_destroy(struct content *c)
{
	struct amiga_dt_picture_content *adt = (struct amiga_dt_picture_content *)c;

	DisposeDTObject(adt->dto);
	adt->dto = NULL;
	
	image_cache_destroy(c);
}

#endif
