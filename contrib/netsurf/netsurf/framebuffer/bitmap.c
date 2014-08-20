/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#include <inttypes.h>
#include <sys/types.h>
#include <stdbool.h>
#include <assert.h>

#include <libnsfb.h>

#include "image/bitmap.h"
#include "utils/log.h"

/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

void *bitmap_create(int width, int height, unsigned int state)
{
        nsfb_t *bm;

        LOG(("width %d, height %d, state %u",width,height,state));

	bm = nsfb_new(NSFB_SURFACE_RAM);
	if (bm == NULL) {
		return NULL;
	}

	if ((state & BITMAP_OPAQUE) == 0) {
		nsfb_set_geometry(bm, width, height, NSFB_FMT_ABGR8888);
	} else {
		nsfb_set_geometry(bm, width, height, NSFB_FMT_XBGR8888);
	}

	if (nsfb_init(bm) == -1) {
		nsfb_free(bm);
		return NULL;		
	}

        LOG(("bitmap %p", bm));

        return bm;
}


/**
 * Return a pointer to the pixel data in a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return pointer to the pixel buffer
 *
 * The pixel data is packed as BITMAP_FORMAT, possibly with padding at the end
 * of rows. The width of a row in bytes is given by bitmap_get_rowstride().
 */

unsigned char *bitmap_get_buffer(void *bitmap)
{
	nsfb_t *bm = bitmap;
	unsigned char *bmpptr;

	assert(bm != NULL);

	nsfb_get_buffer(bm, &bmpptr, NULL);

	return bmpptr;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(void *bitmap)
{
	nsfb_t *bm = bitmap;
	int bmpstride;

	assert(bm != NULL);

	nsfb_get_buffer(bm, NULL, &bmpstride);

	return bmpstride;
}


/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *bitmap)
{
	nsfb_t *bm = bitmap;

	assert(bm != NULL);

	nsfb_free(bm);
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path    pathname for file
 * \return true on success, false on error and error reported
 */

bool bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(void *bitmap) {
}

/**
 * Sets wether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(void *bitmap, bool opaque)
{	
	nsfb_t *bm = bitmap;

	assert(bm != NULL);

        LOG(("setting bitmap %p to %s", bm, opaque?"opaque":"transparent"));

	if (opaque) {
		nsfb_set_geometry(bm, 0, 0, NSFB_FMT_XBGR8888);
	} else {
		nsfb_set_geometry(bm, 0, 0, NSFB_FMT_ABGR8888);
	}
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(void *bitmap)
{
        int tst;
	nsfb_t *bm = bitmap;
	unsigned char *bmpptr;
	int width;
	int height;

	assert(bm != NULL);

	nsfb_get_buffer(bm, &bmpptr, NULL);

	nsfb_get_geometry(bm, &width, &height, NULL);

        tst = width * height;

        while (tst-- > 0) {
                if (bmpptr[(tst << 2) + 3] != 0xff) {
                        LOG(("bitmap %p has transparency",bm));
                        return false;                     
                }   
        }
        LOG(("bitmap %p is opaque", bm));
	return true;
}


/**
 * Gets weather a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *bitmap)
{
	nsfb_t *bm = bitmap;
	enum nsfb_format_e format;

	assert(bm != NULL);

	nsfb_get_geometry(bm, NULL, NULL, &format);

	if (format == NSFB_FMT_ABGR8888)
		return false;

	return true;
}

int bitmap_get_width(void *bitmap)
{
	nsfb_t *bm = bitmap;
	int width;

	assert(bm != NULL);

	nsfb_get_geometry(bm, &width, NULL, NULL);

	return(width);
}

int bitmap_get_height(void *bitmap)
{
	nsfb_t *bm = bitmap;
	int height;

	assert(bm != NULL);

	nsfb_get_geometry(bm, NULL, &height, NULL);

	return(height);
}

/* get bytes per pixel */
size_t bitmap_get_bpp(void *bitmap)
{
	return 4;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
