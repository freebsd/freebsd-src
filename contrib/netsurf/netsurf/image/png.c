/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@hotmail.com>
 * Copyright 2008 Daniel Silverstone <dsilvers@netsurf-browser.org>
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

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <png.h>

#include "desktop/plotters.h"

#include "content/content_protected.h"

#include "image/bitmap.h"
#include "image/image_cache.h"
#include "image/png.h"

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

/* accommodate for old versions of libpng (beware security holes!) */

#ifndef png_jmpbuf
#warning you have an antique libpng
#define png_jmpbuf(png_ptr) ((png_ptr)->jmpbuf)
#endif

#if PNG_LIBPNG_VER < 10209
#define png_set_expand_gray_1_2_4_to_8(png) png_set_gray_1_2_4_to_8(png)
#endif

typedef struct nspng_content {
	struct content base; /**< base content type */

	bool no_process_data; /**< Do not continue to process data as it arrives */
	png_structp png;
	png_infop info;
	int interlace;
	struct bitmap *bitmap;	/**< Created NetSurf bitmap */
	size_t rowstride, bpp; /**< Bitmap rowstride and bpp */
	size_t rowbytes; /**< Number of bytes per row */
} nspng_content;

static unsigned int interlace_start[8] = {0, 16, 0, 8, 0, 4, 0};
static unsigned int interlace_step[8] = {28, 28, 12, 12, 4, 4, 0};
static unsigned int interlace_row_start[8] = {0, 0, 4, 0, 2, 0, 1};
static unsigned int interlace_row_step[8] = {8, 8, 8, 4, 4, 2, 2};

/** Callbak error numbers*/
enum nspng_cberr {
	CBERR_NONE = 0, /* no error */
	CBERR_LIBPNG, /* error from png library */
	CBERR_NOPRE, /* no pre-conversion performed */
}; 

/**
 * nspng_warning -- callback for libpng warnings
 */
static void nspng_warning(png_structp png_ptr, png_const_charp warning_message)
{
	LOG(("%s", warning_message));
}

/**
 * nspng_error -- callback for libpng errors
 */
static void nspng_error(png_structp png_ptr, png_const_charp error_message)
{
	LOG(("%s", error_message));
	longjmp(png_jmpbuf(png_ptr), CBERR_LIBPNG);
}

static void nspng_setup_transforms(png_structp png_ptr, png_infop info_ptr)
{
	int bit_depth, color_type;
#if 0
	int intent;
	double gamma;
#endif

	bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);

	/* Set up our transformations */
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
	}

	if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth < 8)) {
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	}

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png_ptr);
	}

	if (bit_depth == 16) {
		png_set_strip_16(png_ptr);
	}

	if (color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb(png_ptr);
	}

	if (!(color_type & PNG_COLOR_MASK_ALPHA)) {
		png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
	}

#if 0
	/* gamma correction - we use 2.2 as our screen gamma
	 * this appears to be correct (at least in respect to !Browse)
	 * see http://www.w3.org/Graphics/PNG/all_seven.html for a test case
	 */
	if (png_get_sRGB(png_ptr, info_ptr, &intent)) {
		png_set_gamma(png_ptr, 2.2, 0.45455);
	} else {
		if (png_get_gAMA(png_ptr, info_ptr, &gamma)) {
			png_set_gamma(png_ptr, 2.2, gamma);
		} else {
			png_set_gamma(png_ptr, 2.2, 0.45455);
		}
	}
#endif

	png_read_update_info(png_ptr, info_ptr);
}

/**
 * info_callback -- PNG header has been completely received, prepare to process
 * image data
 */
static void info_callback(png_structp png_s, png_infop info)
{
	int interlace;
	png_uint_32 width, height;
	nspng_content *png_c = png_get_progressive_ptr(png_s);

	width = png_get_image_width(png_s, info);
	height = png_get_image_height(png_s, info);
	interlace = png_get_interlace_type(png_s, info);

	png_c->base.width = width;
	png_c->base.height = height;
	png_c->base.size += width * height * 4;

	/* see if progressive-conversion should continue */
	if (image_cache_speculate((struct content *)png_c) == false) {
		longjmp(png_jmpbuf(png_s), CBERR_NOPRE);
	}

	/* Claim the required memory for the converted PNG */
	png_c->bitmap = bitmap_create(width, height, BITMAP_NEW);
	if (png_c->bitmap == NULL) {
		/* Failed to create bitmap skip pre-conversion */
		longjmp(png_jmpbuf(png_s), CBERR_NOPRE);
	}

	png_c->rowstride = bitmap_get_rowstride(png_c->bitmap);
	png_c->bpp = bitmap_get_bpp(png_c->bitmap);

	nspng_setup_transforms(png_s, info);

	png_c->rowbytes = png_get_rowbytes(png_s, info);
	png_c->interlace = (interlace == PNG_INTERLACE_ADAM7);

	LOG(("size %li * %li, rowbytes %zu", (unsigned long)width,
	     (unsigned long)height, png_c->rowbytes));
}

static void row_callback(png_structp png_s, png_bytep new_row,
			 png_uint_32 row_num, int pass)
{
	nspng_content *png_c = png_get_progressive_ptr(png_s);
	unsigned long rowbytes = png_c->rowbytes;
	unsigned char *buffer, *row;

	/* Give up if there's no bitmap */
	if (png_c->bitmap == NULL)
		return;

	/* Abort if we've not got any data */
	if (new_row == NULL)
		return;

	/* Get bitmap buffer */
	buffer = bitmap_get_buffer(png_c->bitmap);
	if (buffer == NULL) {
		/* No buffer, bail out */
		longjmp(png_jmpbuf(png_s), 1);
	}

	/* Calculate address of row start */
	row = buffer + (png_c->rowstride * row_num);

	/* Handle interlaced sprites using the Adam7 algorithm */
	if (png_c->interlace) {
		unsigned long dst_off;
		unsigned long src_off = 0;
		unsigned int start, step;

		start = interlace_start[pass];
		step = interlace_step[pass];
		row_num = interlace_row_start[pass] +
			interlace_row_step[pass] * row_num;

		/* Copy the data to our current row taking interlacing
		 * into consideration */
		row = buffer + (png_c->rowstride * row_num);

		for (dst_off = start; dst_off < rowbytes; dst_off += step) {
			row[dst_off++] = new_row[src_off++];
			row[dst_off++] = new_row[src_off++];
			row[dst_off++] = new_row[src_off++];
			row[dst_off++] = new_row[src_off++];
		}
	} else {
		/* Do a fast memcpy of the row data */
		memcpy(row, new_row, rowbytes);
	}
}


static void end_callback(png_structp png_s, png_infop info)
{
}

static nserror nspng_create_png_data(nspng_content *png_c)
{
	union content_msg_data msg_data;

	png_c->bitmap = NULL;

	png_c->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (png_c->png == NULL) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(&png_c->base, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return NSERROR_NOMEM;
	}

	png_set_error_fn(png_c->png, NULL, nspng_error, nspng_warning);

	png_c->info = png_create_info_struct(png_c->png);
	if (png_c->info == NULL) {
		png_destroy_read_struct(&png_c->png, &png_c->info, 0);

		msg_data.error = messages_get("NoMemory");
		content_broadcast(&png_c->base, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return NSERROR_NOMEM;
	}

	if (setjmp(png_jmpbuf(png_c->png))) {
		png_destroy_read_struct(&png_c->png, &png_c->info, 0);
		LOG(("Failed to set callbacks"));
		png_c->png = NULL;
		png_c->info = NULL;

		msg_data.error = messages_get("PNGError");
		content_broadcast(&png_c->base, CONTENT_MSG_ERROR, msg_data);
		return NSERROR_NOMEM;
	}

	png_set_progressive_read_fn(png_c->png, png_c,
			info_callback, row_callback, end_callback);

	return NSERROR_OK;
}

static nserror nspng_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	nspng_content *png_c;
	nserror error;

	png_c = calloc(1, sizeof(nspng_content));
	if (png_c == NULL)
		return NSERROR_NOMEM;

	error = content__init(&png_c->base, 
			      handler, 
			      imime_type, 
			      params,
			      llcache, 
			      fallback_charset, 
			      quirks);
	if (error != NSERROR_OK) {
		free(png_c);
		return error;
	}

	error = nspng_create_png_data(png_c);
	if (error != NSERROR_OK) {
		free(png_c);
		return error;
	}

	*c = (struct content *)png_c;

	return NSERROR_OK;
}


static bool nspng_process_data(struct content *c, const char *data,
			       unsigned int size)
{
	nspng_content *png_c = (nspng_content *)c;
	union content_msg_data msg_data;
	volatile bool ret = true;

	if (png_c->no_process_data) {
		return ret;
	}

	switch (setjmp(png_jmpbuf(png_c->png))) {
	case CBERR_NONE: /* direct return */	
		png_process_data(png_c->png, png_c->info, (uint8_t *)data, size);
		break;

	case CBERR_NOPRE: /* not going to progressive convert */
		png_c->no_process_data = true;
		break;

	default: /* fatal error from library processing png */
		if (png_c->bitmap != NULL) {
			/* A bitmap managed to get created so
			 * operation is past header and possibly some
			 * conversion happened before faliure. 
			 *
			 * In this case keep the partial
			 * conversion. This is usually seen if a png
			 * has been truncated (often jsut lost its
			 * last byte and hence end of image marker)
			 */
			png_c->no_process_data = true;
		} else {
			/* not managed to progress past header, clean
			 * up png conversion and signal the content
			 * error 
			 */
			LOG(("Fatal PNG error during header, error content"));

			png_destroy_read_struct(&png_c->png, &png_c->info, 0);
			png_c->png = NULL;
			png_c->info = NULL;

			msg_data.error = messages_get("PNGError");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);

			ret = false;

		}
		break;
	}

	return ret;
}

struct png_cache_read_data_s {
	const char *data;
	unsigned long size;
};

/** PNG library read fucntion to read data from a memory array 
 */
static void 
png_cache_read_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
	struct png_cache_read_data_s *png_cache_read_data;
	png_cache_read_data = png_get_io_ptr(png_ptr);

	if (length > png_cache_read_data->size) {
		length = png_cache_read_data->size;
	}

	if (length == 0) {
		png_error(png_ptr, "Read Error");
	}

	memcpy(data, png_cache_read_data->data, length);

	png_cache_read_data->data += length;
	png_cache_read_data->size -= length;
}

/** calculate an array of row pointers into a bitmap data area
 */
static png_bytep *calc_row_pointers(struct bitmap *bitmap)
{
	int height = bitmap_get_height(bitmap);
	unsigned char *buffer= bitmap_get_buffer(bitmap);
	size_t rowstride = bitmap_get_rowstride(bitmap);
	png_bytep *row_ptrs;
	int hloop;

	row_ptrs = malloc(sizeof(png_bytep) * height);

	if (row_ptrs != NULL) {
		for (hloop = 0; hloop < height; hloop++) {
			row_ptrs[hloop] = buffer + (rowstride * hloop);
		}
	}

	return row_ptrs;
}

/** PNG content to bitmap conversion.
 *
 * This routine generates a bitmap object from a PNG image content
 */
static struct bitmap *
png_cache_convert(struct content *c)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info_ptr;
	volatile struct bitmap *bitmap = NULL;
	struct png_cache_read_data_s png_cache_read_data;
	png_uint_32 width, height;
	volatile png_bytep *row_pointers = NULL;

	png_cache_read_data.data = 
		content__get_source_data(c, &png_cache_read_data.size);

	if ((png_cache_read_data.data == NULL) || 
	    (png_cache_read_data.size <= 8)) {
		return NULL;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
			nspng_error, nspng_warning);
	if (png_ptr == NULL) {
		return NULL;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return NULL;
	}

	end_info_ptr = png_create_info_struct(png_ptr);
	if (end_info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	/* setup error exit path */
	if (setjmp(png_jmpbuf(png_ptr))) {
		/* cleanup and bail */
		goto png_cache_convert_error;
	}

	/* read from a buffer instead of stdio */
	png_set_read_fn(png_ptr, &png_cache_read_data, png_cache_read_fn);

	/* ensure the png info structure is populated */
	png_read_info(png_ptr, info_ptr);

	/* setup output transforms */
	nspng_setup_transforms(png_ptr, info_ptr);

	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);

	/* Claim the required memory for the converted PNG */
	bitmap = bitmap_create(width, height, BITMAP_NEW);
	if (bitmap == NULL) {
		/* cleanup and bail */
		goto png_cache_convert_error;
	}

	row_pointers = calc_row_pointers((struct bitmap *) bitmap);

	if (row_pointers != NULL) {
		png_read_image(png_ptr, (png_bytep *) row_pointers);
	}

png_cache_convert_error:

	/* cleanup png read */
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);

	free((png_bytep *) row_pointers);

	if (bitmap != NULL)
		bitmap_modified((struct bitmap *)bitmap);

	return (struct bitmap *)bitmap;
}

static bool nspng_convert(struct content *c)
{
	nspng_content *png_c = (nspng_content *) c;
	char *title;

	assert(png_c->png != NULL);
	assert(png_c->info != NULL);

	/* clean up png structures */
	png_destroy_read_struct(&png_c->png, &png_c->info, 0);

	/* set title text */
	title = messages_get_buff("PNGTitle",
			nsurl_access_leaf(llcache_handle_get_url(c->llcache)),
			c->width, c->height);
	if (title != NULL) {
		content__set_title(c, title);
		free(title);
	}

	if (png_c->bitmap != NULL) {
		bitmap_set_opaque(png_c->bitmap, bitmap_test_opaque(png_c->bitmap));
		bitmap_modified(png_c->bitmap);
	}

	image_cache_add(c, png_c->bitmap, png_cache_convert);

	content_set_ready(c);
	content_set_done(c);
	content_set_status(c, "");

	return true;
}


static nserror nspng_clone(const struct content *old_c, struct content **new_c)
{
	nspng_content *clone_png_c;
	nserror error;
	const char *data;
	unsigned long size;

	clone_png_c = calloc(1, sizeof(nspng_content));
	if (clone_png_c == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old_c, &clone_png_c->base);
	if (error != NSERROR_OK) {
		content_destroy(&clone_png_c->base);
		return error;
	}

	/* Simply replay create/process/convert */
	error = nspng_create_png_data(clone_png_c);
	if (error != NSERROR_OK) {
		content_destroy(&clone_png_c->base);
		return error;
	}

	data = content__get_source_data(&clone_png_c->base, &size);
	if (size > 0) {
		if (nspng_process_data(&clone_png_c->base, data, size) == false) {
			content_destroy(&clone_png_c->base);
			return NSERROR_NOMEM;
		}
	}

	if ((old_c->status == CONTENT_STATUS_READY) ||
	    (old_c->status == CONTENT_STATUS_DONE)) {
		if (nspng_convert(&clone_png_c->base) == false) {
			content_destroy(&clone_png_c->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*new_c = (struct content *)clone_png_c;

	return NSERROR_OK;
}

static const content_handler nspng_content_handler = {
	.create = nspng_create,
	.process_data = nspng_process_data,
	.data_complete = nspng_convert,
	.clone = nspng_clone,
	.destroy = image_cache_destroy,
	.redraw = image_cache_redraw,
	.get_internal = image_cache_get_internal,
	.type = image_cache_content_type,
	.no_share = false,
};

static const char *nspng_types[] = {
	"image/png",
	"image/x-png"
};

CONTENT_FACTORY_REGISTER_TYPES(nspng, nspng_types, nspng_content_handler);
