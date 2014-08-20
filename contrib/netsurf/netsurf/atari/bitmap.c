/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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
#include <string.h>


#include "assert.h"
#include "utils/nsoption.h"
#include "image/bitmap.h"
#include "atari/bitmap.h"
#include "atari/plot/plot.h"
#include "utils/log.h"


/*
 * param bpp bits per pixel,
 * param w width of the buffer (in pixel)
 * param h height of the buffer (in pixel)
 * param flags MFDB_FLAG_NOALLOC | MFDB_FLAG_ZEROMEM | MFDB_FLAG_STAND
 * returns size of the fd_addr buffer required or allocated
*/
int init_mfdb(int bpp, int w, int h, uint32_t flags, MFDB * out )
{
	int dststride;
	dststride = MFDB_STRIDE( w );
	int size = MFDB_SIZE( bpp, dststride, h );
	if( bpp > 0 ) {
		if( (flags & MFDB_FLAG_NOALLOC) == 0  ) {
			out->fd_addr = malloc( size );
			if( out->fd_addr == NULL ){
				return( 0 );
			}
			if( (flags & MFDB_FLAG_ZEROMEM) ){
				memset( out->fd_addr, 0, size );
			}
		}
		out->fd_stand = (flags & MFDB_FLAG_STAND) ? 1 : 0;
		out->fd_nplanes = (short)bpp;
		out->fd_r1 = out->fd_r2 = out->fd_r3 = 0;
	} else {
		memset( out, 0, sizeof(MFDB) );
	}
	out->fd_w = dststride;
	out->fd_h = h;
	out->fd_wdwidth = dststride >> 4;
	return( size );
}


/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

void *bitmap_create(int w, int h, unsigned int state)
{
	return bitmap_create_ex( w, h, NS_BMP_DEFAULT_BPP, w * NS_BMP_DEFAULT_BPP, state, NULL );
}

/**
 * Create a bitmap.
 *
 * \param  w	width of image in pixels
 * \param  h 	height of image in pixels
 * \param  bpp	number of BYTES per pixel
 * \param  rowstride 	linewidth in bytes
 * \param  state 	a flag word indicating the initial state
 * \param  pixdata 	NULL or an memory address to use as the bitmap pixdata
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */
void * bitmap_create_ex( int w, int h, short bpp, int rowstride, unsigned int state, void * pixdata )
{
    struct bitmap * bitmap;

    LOG(("width %d (rowstride: %d, bpp: %d), height %d, state %u",w, rowstride,
		bpp, h, state ));

	if( rowstride == 0) {
		rowstride = bpp * w;
	}

	assert( rowstride >= (w * bpp) );
    bitmap = calloc(1 , sizeof(struct bitmap) );
    if (bitmap) {
		if( pixdata == NULL) {
          	bitmap->pixdata = calloc(1, (rowstride * h)+128);
		}
		else {
			bitmap->pixdata = pixdata;
		}

        if (bitmap->pixdata != NULL) {
			bitmap->width = w;
			bitmap->height = h;
			bitmap->opaque = (state & BITMAP_OPAQUE) ? true : false;
			bitmap->bpp = bpp;
			bitmap->resized = NULL;
			bitmap->rowstride = rowstride;
        } else {
			free(bitmap);
			bitmap=NULL;
			LOG(("Out of memory!"));
		}
	}
	LOG(("bitmap %p", bitmap));
	return bitmap;
}

void * bitmap_realloc( int w, int h, short bpp, int rowstride, unsigned int state, void * bmp )
{
	struct bitmap * bitmap = bmp;
	int newsize = rowstride * h;

	if( bitmap == NULL ) {
		return( NULL );
	}

	assert( bitmap->pixdata != NULL );
	int oldsize = bitmap->rowstride * bitmap->height;
	bool doalloc = (state & BITMAP_GROW) ? (newsize > oldsize) : (newsize != oldsize);
	if( newsize > oldsize )
		assert( doalloc == true );
	if( doalloc ) {
		// TODO: set red band to a specific value and check the red band
		// on bitmap_destroy()
		bitmap->pixdata = realloc( bitmap->pixdata, newsize + 128 );
		if( bitmap->pixdata == NULL )
			return( NULL );
	}

	if( state & BITMAP_CLEAR ){
		memset( bitmap->pixdata, 0x00, newsize + 128  );
	}

	bitmap->width = w;
	bitmap->height = h;
	bitmap->bpp = bpp;
	bitmap->resized = NULL;
	bitmap->rowstride = rowstride;
	bitmap_modified( bitmap );
	return( bitmap );
}

void bitmap_to_mfdb(void * bitmap, MFDB * out)
{
	struct bitmap * bm;
	uint8_t * tmp;
	size_t dststride, oldstride;

	bm = bitmap;
	assert( out != NULL );
	assert( bm->pixdata != NULL );

	oldstride = bitmap_get_rowstride( bm );
	dststride = MFDB_STRIDE( bm->width );

	if( oldstride != dststride * bm->bpp )
	{
		assert( oldstride <= dststride );
		/* we need to convert the img to new rowstride */
		tmp = bm->pixdata;
		bm->pixdata = calloc(1, dststride * bm->bpp * bm->height );
		if( tmp == NULL ){
			bm->pixdata = tmp;
			out->fd_addr = NULL;
			return;
		}
		bm->rowstride = dststride * bm->bpp;
		int i=0;
		for( i=0; i<bm->height; i++) {
			memcpy( (bm->pixdata+i*bm->rowstride), (tmp + i*oldstride), oldstride);
		}
		free( tmp );
	}
	out->fd_w = dststride;
	out->fd_h = bm->height;
	out->fd_wdwidth = dststride >> 4;
	out->fd_addr = bm->pixdata;
	out->fd_stand = 0;
	out->fd_nplanes = (short)bm->bpp;
	out->fd_r1 = out->fd_r2 = out->fd_r3 = 0;
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

unsigned char * bitmap_get_buffer(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return NULL;
	}

	return bm->pixdata;
}

size_t bitmap_buffer_size( void * bitmap )
{
	struct bitmap * bm = bitmap;
	if( bm == NULL )
		return 0;
	return( bm->rowstride * bm->height );
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return 0;
	}
	return bm->rowstride;
}


/**
 * Free a bitmap.
 *
 * \param  bitmap a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return;
	}

	if( bm->resized != NULL ) {
		bitmap_destroy(bm->resized);
	}
	if( bm->converted && ( bm->native.fd_addr != bm->pixdata ) )
		free( bm->native.fd_addr );
	free(bm->pixdata);
	free(bm);
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
void bitmap_modified(void *bitmap)
{
	struct bitmap *bm = bitmap;
	if( bm->resized != NULL ) {
		bitmap_destroy( bm->resized );
		bm->resized = NULL;
	}
	if( bm->converted ){
		if( bm->pixdata != bm->native.fd_addr ){
			free( bm->native.fd_addr );
		}
		bm->native.fd_addr = NULL;
		bm->converted = false;
	}

}

/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(void *bitmap, bool opaque)
{
	struct bitmap *bm = bitmap;

    if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return;
	}

	LOG(("setting bitmap %p to %s", bm, opaque?"opaque":"transparent"));
    bm->opaque = opaque;
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
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return false;
	}

    if( nsoption_int(atari_transparency) == 0 ){
        return( true );
    }

	tst = bm->width * bm->height;

	while (tst-- > 0) {
		if (bm->pixdata[(tst << 2) + 3] != 0xff) {
				LOG(("bitmap %p has transparency",bm));
					return false;
		}
	}
	LOG(("bitmap %p is opaque", bm));
	return true;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *bitmap)
{
	struct bitmap *bm = bitmap;

        if (bitmap == NULL) {
                LOG(("NULL bitmap!"));
                return false;
        }

	return bm->opaque;
}

int bitmap_get_width(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return 0;
	}

	return(bm->width);
}

int bitmap_get_height(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return 0;
	}
	return(bm->height);
}


/**
*
*	Gets the number of BYTES per pixel.
*
*/
size_t bitmap_get_bpp(void *bitmap)
{
	struct bitmap *bm = bitmap;
	return bm->bpp;
}

bool bitmap_resize(struct bitmap *img, HermesHandle hermes_h,
		HermesFormat *fmt, int nw, int nh)
{
	unsigned int state = 0;
	short bpp = bitmap_get_bpp( img );
	int stride = bitmap_get_rowstride( img );
	int err;

	if( img->resized != NULL ) {
		if( img->resized->width != nw || img->resized->height != nh ) {
			bitmap_destroy( img->resized );
			img->resized = NULL;
		} else {
			/* the bitmap is already resized */
			return(true);
		}
	}

	/* allocate the mem for resized bitmap */
	if (img->opaque == true) {
		state |= BITMAP_OPAQUE;
	}
	img->resized = bitmap_create_ex( nw, nh, bpp, nw*bpp, state, NULL );
	if( img->resized == NULL ) {
			printf("W: %d, H: %d, bpp: %d\n", nw, nh, bpp);
			assert(img->resized);
			return(false);
	}

	/* allocate an converter, only for resizing */
	err = Hermes_ConverterRequest( hermes_h,
			fmt,
			fmt
	);
	if( err == 0 ) {
		return(false);
	}

	err = Hermes_ConverterCopy( hermes_h,
		img->pixdata,
		0,			/* x src coord of top left in pixel coords */
		0,			/* y src coord of top left in pixel coords */
		bitmap_get_width( img ), bitmap_get_height( img ),
		stride, 	/* stride as bytes */
		img->resized->pixdata,
		0,			/* x dst coord of top left in pixel coords */
		0,			/* y dst coord of top left in pixel coords */
		nw, nh,
		bitmap_get_rowstride(img->resized) /* stride as bytes */
	);
	if( err == 0 ) {
		bitmap_destroy( img->resized );
		img->resized = NULL;
		return(false);
	}

	return(true);
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
