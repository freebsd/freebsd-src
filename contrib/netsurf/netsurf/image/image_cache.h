/*
 * Copyright 2011 John-Mark Bell <jmb@netsurf-browser.org>
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
 * The image content handler intermediate image cache.
 *
 * This cache allows netsurf to use a generic intermediate bitmap
 * format without keeping the
 * intermediate representation in memory.
 *
 * The bitmap structure is opaque to the rest of netsurf and is
 * controlled by the platform-specific code (see image/bitmap.h for
 * detials). All image content handlers convert into this format and
 * pass it to the plot functions for display,
 * 
 * This cache maintains a link between the underlying original content
 * and the intermediate representation. It is intended to be flexable
 * and either manage the bitmap plotting completely or give the image
 * content handler complete control.
 */

#ifndef NETSURF_IMAGE_IMAGE_CACHE_H_
#define NETSURF_IMAGE_IMAGE_CACHE_H_

#include "utils/errors.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"

typedef struct bitmap * (image_cache_convert_fn) (struct content *content);

struct image_cache_parameters {
	/** How frequently the background cache clean process is run (ms) */
	unsigned int bg_clean_time;

	/** The target upper bound for the image cache size */
	size_t limit;

	/** The hysteresis allowed round the target size */
	size_t hysteresis;

	/** The speculative conversion "small" size */
	size_t speculative_small;
};

/** Initialise the image cache 
 *
 * @param image_cache_parameters The control parameters for the image cache
 */
nserror image_cache_init(const struct image_cache_parameters *image_cache_parameters);
nserror image_cache_fini(void);

/** adds an image content to be cached. 
 * 
 * @param content The content handle used as a key
 * @param bitmap A bitmap representing the already converted content or NULL.
 * @param convert A function pointer to convert the content into a bitmap or NULL.
 * @return A netsurf error code.
 */
nserror image_cache_add(struct content *content, 
			struct bitmap *bitmap, 
			image_cache_convert_fn *convert);

nserror image_cache_remove(struct content *content);


/** Obtain a bitmap from a content converting from source if neccessary. */
struct bitmap *image_cache_get_bitmap(const struct content *c);

/** Obtain a bitmap from a content with no conversion */
struct bitmap *image_cache_find_bitmap(struct content *c);

/** Decide if a content should be speculatively converted.
 *
 * This allows for image content handlers to ask the cache if a bitmap
 * should be generated before it is added to the cache. This is the
 * same decision logic used to decide to perform an immediate
 * conversion when a content is initially added to the cache. 
 *
 * @param c The content to be considered.
 * @return true if a speculative conversion is desired false otehrwise.
 */
bool image_cache_speculate(struct content *c);

/**
 * Fill a buffer with information about a cache entry using a format.
 *
 * The format string is copied into the output buffer with the
 * following replaced:
 * %e - The entry number
 * %k - The content key
 * %r - The number of redraws of this bitmap
 * %c - The number of times this bitmap has been converted
 * %s - The size of the current bitmap allocation
 *
 * \param string  The buffer in which to place the results.
 * \param size    The size of the string buffer.
 * \param entryn  The opaque entry number.
 * \param fmt     The format string.
 * \return The number of bytes written to \a string or -1 on error
 */
int image_cache_snentryf(char *string, size_t size, unsigned int entryn,
			 const char *fmt);

/**
 * Fill a buffer with information about the image cache using a format.
 *
 * The format string is copied into the output buffer with the
 * following replaced:
 *
 * a Configured cache limit size
 * b Configured cache hysteresis size
 * c Current caches total consumed size
 * d Number of images currently in the cache
 * e The age of the cache
 * f The largest amount of space the cache has occupied since initialisation
 * g The number of objetcs when the cache was at its largest
 * h The largest number of images in the cache since initialisation
 * i The size of the cache when the largest number of objects occoured
 * j The total number of read operations performed on the cache
 * k The total number of read operations satisfied from the cache without 
 *     conversion.
 * l The total number of read operations satisfied from the cache which 
 *     required a conversion.
 * m The total number of read operations which could not be sucessfully 
 *     returned. ie. not available in cache and conversion failed.
 * n The total size  of read operations performed on the cache
 * o The total size of read operations satisfied from the cache without 
 *     conversion.
 * q The total size of read operations satisfied from the cache which 
 *     required a conversion.
 * r The total size of read operations which could not be sucessfully 
 *     returned. ie. not available in cache and conversion failed.
 * s The number of images which were placed in the cache but never read.
 * t The number of images that were converted on insertion into teh cache which were subsequently never used.
 * u The number of times an image was converted after the first
 * v The number of images that had extra conversions performed.
 * w Size of the image that was converted (read missed cache) highest number 
 *     of times.
 * x The number of times the image that was converted (read missed cache) 
 *     highest number of times.
 *
 * format modifiers:
 * A p before the value modifies the replacement to be a percentage.
 *
 *
 * \param string  The buffer in which to place the results.
 * \param size    The size of the string buffer.
 * \param fmt     The format string.
 * \return The number of bytes written to \a string or -1 on error
 */

int image_cache_snsummaryf(char *string, size_t size, const char *fmt);

/********* Image content handler generic cache callbacks ************/

/** Generic content redraw callback
 *
 * May be used by image content handlers as their redraw
 * callback. Performs all neccissary cache lookups and conversions and
 * calls the bitmap plot function in the redraw context.
 */
bool image_cache_redraw(struct content *c, 
			struct content_redraw_data *data,
			const struct rect *clip, 
			const struct redraw_context *ctx);

void image_cache_destroy(struct content *c);

void *image_cache_get_internal(const struct content *c, void *context);

content_type image_cache_content_type(void);

#endif
