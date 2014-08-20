/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
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
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "desktop/gui_factory.h"
#include "utils/log.h"
#include "content/content_protected.h"

#include "image/image_cache.h"
#include "image/image.h"

/** Age of an entry within the cache
 *
 * type deffed away so it can be readily changed later perhaps to a
 * wallclock time structure.
 */
typedef unsigned int cache_age;

/** Image cache entry
 */
struct image_cache_entry_s {
	struct image_cache_entry_s *next; /* next cache entry in list */
	struct image_cache_entry_s *prev; /* previous cache entry in list */

	struct content *content; /** content is used as a key */
	struct bitmap *bitmap; /** associated bitmap entry */
	/** Conversion routine */
	image_cache_convert_fn *convert;

	/* Statistics for replacement algorithm */

	unsigned int redraw_count; /**< number of times object has been drawn */
	cache_age redraw_age; /**< Age of last redraw */
	size_t bitmap_size; /**< size if storage occupied by bitmap */
	cache_age bitmap_age; /**< Age of last conversion to a bitmap by cache*/

	int conversion_count; /**< Number of times image has been converted */
};

/** Current state of the cache.
 *
 * Global state of the cache. entries "age" is determined based on a
 * monotonically incrementing operation count. This avoids issues with
 * using wall clock time while allowing the LRU algorithm to work
 * sensibly.
 */
struct image_cache_s {
	/** Cache parameters */
	struct image_cache_parameters params;

	/** The "age" of the current operation */
	cache_age current_age;

	/* The objects the cache holds */
	struct image_cache_entry_s *entries;


	/* Statistics for management algorithm */

	/** total size of bitmaps currently allocated */
	size_t total_bitmap_size;

	/** Total count of bitmaps currently allocated */
	int bitmap_count;

	/** Maximum size of bitmaps allocated at any one time */
	size_t max_bitmap_size;
	/** The number of objects when maximum bitmap usage occoured */
	int max_bitmap_size_count;

	/** Maximum count of bitmaps allocated at any one time */
	int max_bitmap_count;
	/** The size of the bitmaps when the max count occoured */
	size_t max_bitmap_count_size;

	/** Bitmap was not available at plot time required conversion */
	int miss_count;
	uint64_t miss_size;
	/** Bitmap was available at plot time required no conversion */
	int hit_count;
	uint64_t hit_size;
	/** Bitmap was not available at plot time and required
	 * conversion which failed.
	 */
	int fail_count;
	uint64_t fail_size;

	/* Cache entry freed without ever being redrawn */
	int total_unrendered;
	/** Bitmap was available but never required - wasted conversions */
	int specultive_miss_count;

	/** Total number of additional (after the first) conversions */
	int total_extra_conversions;
	/** counts total number of images with more than one conversion */
	int total_extra_conversions_count;

	/** Bitmap with most conversions was converted this many times */
	int peak_conversions;
	/** Size of bitmap with most conversions */
	unsigned int peak_conversions_size;
};

/** image cache state */
static struct image_cache_s *image_cache = NULL;


/** Find the nth cache entry
 */
static struct image_cache_entry_s *image_cache__findn(int entryn)
{
	struct image_cache_entry_s *found;

	found = image_cache->entries;
	while ((found != NULL) && (entryn > 0)) {
		entryn--;
		found = found->next;
	}
	return found;
}

/** Find the cache entry for a content
 */
static struct image_cache_entry_s *image_cache__find(const struct content *c)
{
	struct image_cache_entry_s *found;

	found = image_cache->entries;
	while ((found != NULL) && (found->content != c)) {
		found = found->next;
	}
	return found;
}

static void image_cache_stats_bitmap_add(struct image_cache_entry_s *centry)
{
	centry->bitmap_age = image_cache->current_age;
	centry->conversion_count++;

	image_cache->total_bitmap_size += centry->bitmap_size;
	image_cache->bitmap_count++;

	if (image_cache->total_bitmap_size > image_cache->max_bitmap_size) {
		image_cache->max_bitmap_size = image_cache->total_bitmap_size;
		image_cache->max_bitmap_size_count = image_cache->bitmap_count;

	}

	if (image_cache->bitmap_count > image_cache->max_bitmap_count) {
		image_cache->max_bitmap_count = image_cache->bitmap_count;
		image_cache->max_bitmap_count_size = image_cache->total_bitmap_size;
	}

	if (centry->conversion_count == 2) {
		image_cache->total_extra_conversions_count++;
	}

	if (centry->conversion_count > 1) {
		image_cache->total_extra_conversions++;
	}

	if ((centry->conversion_count > image_cache->peak_conversions) ||
	    (centry->conversion_count == image_cache->peak_conversions &&
	     centry->bitmap_size > image_cache->peak_conversions_size)) {
		image_cache->peak_conversions = centry->conversion_count;
		image_cache->peak_conversions_size = centry->bitmap_size;
	}
}

static void image_cache__link(struct image_cache_entry_s *centry)
{
	centry->next = image_cache->entries;
	centry->prev = NULL;
	if (centry->next != NULL) {
		centry->next->prev = centry;
	}
	image_cache->entries = centry;
}

static void image_cache__unlink(struct image_cache_entry_s *centry)
{
	/* unlink entry */
	if (centry->prev == NULL) {
		/* first in list */
		if (centry->next != NULL) {
			centry->next->prev = centry->prev;
			image_cache->entries = centry->next;
		} else {
			/* empty list */
			image_cache->entries = NULL;
		}
	} else {
		centry->prev->next = centry->next;

		if (centry->next != NULL) {
			centry->next->prev = centry->prev;
		}
	}
}

static void image_cache__free_bitmap(struct image_cache_entry_s *centry)
{
	if (centry->bitmap != NULL) {
#ifdef IMAGE_CACHE_VERBOSE
		LOG(("Freeing bitmap %p size %d age %d redraw count %d",
		     centry->bitmap,
		     centry->bitmap_size,
		     image_cache->current_age - centry->bitmap_age,
		     centry->redraw_count));
#endif
		bitmap_destroy(centry->bitmap);
		centry->bitmap = NULL;
		image_cache->total_bitmap_size -= centry->bitmap_size;
		image_cache->bitmap_count--;
		if (centry->redraw_count == 0) {
			image_cache->specultive_miss_count++;
		}
	}

}

/* free cache entry */
static void image_cache__free_entry(struct image_cache_entry_s *centry)
{
#ifdef IMAGE_CACHE_VERBOSE
	LOG(("freeing %p ", centry));
#endif

	if (centry->redraw_count == 0) {
		image_cache->total_unrendered++;
	}

	image_cache__free_bitmap(centry);

	image_cache__unlink(centry);

	free(centry);
}

/** Cache cleaner */
static void image_cache__clean(struct image_cache_s *icache)
{
	struct image_cache_entry_s *centry = icache->entries;

	while (centry != NULL) {
		if ((icache->current_age - centry->redraw_age) >
		    icache->params.bg_clean_time) {
			/* only consider older entries, avoids active entries */
			if ((icache->total_bitmap_size >
			     (icache->params.limit - icache->params.hysteresis)) &&
			    (rand() > (RAND_MAX / 2))) {
				image_cache__free_bitmap(centry);
			}
		}
		centry=centry->next;
	}
}

/** Cache background scheduled callback. */
static void image_cache__background_update(void *p)
{
	struct image_cache_s *icache = p;

	/* increment current cache age */
	icache->current_age += icache->params.bg_clean_time;

#ifdef IMAGE_CACHE_VERBOSE
	LOG(("Cache age %ds", icache->current_age / 1000));
#endif

	image_cache__clean(icache);

	guit->browser->schedule(icache->params.bg_clean_time,
				image_cache__background_update,
				icache);
}

/* exported interface documented in image_cache.h */
struct bitmap *image_cache_get_bitmap(const struct content *c)
{
	struct image_cache_entry_s *centry;

	centry = image_cache__find(c);
	if (centry == NULL) {
		return NULL;
	}

	if (centry->bitmap == NULL) {
		if (centry->convert != NULL) {
			centry->bitmap = centry->convert(centry->content);
		}

		if (centry->bitmap != NULL) {
			image_cache_stats_bitmap_add(centry);
			image_cache->miss_count++;
			image_cache->miss_size += centry->bitmap_size;
		} else {
			image_cache->fail_count++;
			image_cache->fail_size += centry->bitmap_size;
		}
	} else {
		image_cache->hit_count++;
		image_cache->hit_size += centry->bitmap_size;
	}

	return centry->bitmap;
}

/* exported interface documented in image_cache.h */
bool image_cache_speculate(struct content *c)
{
	bool decision = false;

	/* If the cache is below its target usage and the bitmap is
	 * small enough speculate.
	 */
	if ((image_cache->total_bitmap_size < image_cache->params.limit) &&
	    (c->size <= image_cache->params.speculative_small)) {
#ifdef IMAGE_CACHE_VERBOSE
		LOG(("content size (%d) is smaller than minimum (%d)", c->size, SPECULATE_SMALL));
#endif
		decision = true;
	}

#ifdef IMAGE_CACHE_VERBOSE
	LOG(("returning %d", decision));
#endif
	return decision;
}

/* exported interface documented in image_cache.h */
struct bitmap *image_cache_find_bitmap(struct content *c)
{
	struct image_cache_entry_s *centry;

	centry = image_cache__find(c);
	if (centry == NULL) {
		return NULL;
	}

	return centry->bitmap;
}

/* exported interface documented in image_cache.h */
nserror
image_cache_init(const struct image_cache_parameters *image_cache_parameters)
{
	image_cache = calloc(1, sizeof(struct image_cache_s));
	if (image_cache == NULL) {
		return NSERROR_NOMEM;
	}

	image_cache->params = *image_cache_parameters;

	guit->browser->schedule(image_cache->params.bg_clean_time,
				image_cache__background_update,
				image_cache);

	LOG(("Image cache initilised with a limit of %d hysteresis of %d",
	     image_cache->params.limit, image_cache->params.hysteresis));

	return NSERROR_OK;
}

/* exported interface documented in image_cache.h */
nserror image_cache_fini(void)
{
	unsigned int op_count;

	guit->browser->schedule(-1, image_cache__background_update, image_cache);

	LOG(("Size at finish %d (in %d)",
	     image_cache->total_bitmap_size,
	     image_cache->bitmap_count));

	while (image_cache->entries != NULL) {
		image_cache__free_entry(image_cache->entries);
	}

	op_count = image_cache->hit_count +
		image_cache->miss_count +
		image_cache->fail_count;

	LOG(("Age %ds", image_cache->current_age / 1000));
	LOG(("Peak size %d (in %d)",
	     image_cache->max_bitmap_size,
	     image_cache->max_bitmap_size_count ));
	LOG(("Peak image count %d (size %d)",
	     image_cache->max_bitmap_count,
	     image_cache->max_bitmap_count_size));

	if (op_count > 0) {
		uint64_t op_size;

		op_size = image_cache->hit_size +
			image_cache->miss_size +
			image_cache->fail_size;

		LOG(("Cache total/hit/miss/fail (counts) %d/%d/%d/%d (100%%/%d%%/%d%%/%d%%)",
		     op_count,
		     image_cache->hit_count,
		     image_cache->miss_count,
		     image_cache->fail_count,
		     (image_cache->hit_count * 100) / op_count,
		     (image_cache->miss_count * 100) / op_count,
		     (image_cache->fail_count * 100) / op_count));
		LOG(("Cache total/hit/miss/fail (size) %d/%d/%d/%d (100%%/%d%%/%d%%/%d%%)",
		     op_size,
		     image_cache->hit_size,
		     image_cache->miss_size,
		     image_cache->fail_size,
		     (image_cache->hit_size * 100) / op_size,
		     (image_cache->miss_size * 100) / op_size,
		     (image_cache->fail_size * 100) / op_size));
	}

	LOG(("Total images never rendered: %d (includes %d that were converted)",
	     image_cache->total_unrendered,
	     image_cache->specultive_miss_count));

	LOG(("Total number of excessive conversions: %d (from %d images converted more than once)",
	     image_cache->total_extra_conversions,
	     image_cache->total_extra_conversions_count));

	LOG(("Bitmap of size %d had most (%d) conversions",
	     image_cache->peak_conversions_size,
	     image_cache->peak_conversions));

	free(image_cache);

	return NSERROR_OK;
}

/* exported interface documented in image_cache.h */
nserror image_cache_add(struct content *content,
			struct bitmap *bitmap,
			image_cache_convert_fn *convert)
{
	struct image_cache_entry_s *centry;

	/* bump the cache age by a ms to ensure multiple items are not
	 * added at exactly the same time
	 */
	image_cache->current_age++;

	centry = image_cache__find(content);
	if (centry == NULL) {
		/* new cache entry, content not previously added */
		centry = calloc(1, sizeof(struct image_cache_entry_s));
		if (centry == NULL) {
			return NSERROR_NOMEM;
		}
		image_cache__link(centry);
		centry->content = content;

		centry->bitmap_size = content->width * content->height * 4;
	}

	LOG(("centry %p, content %p, bitmap %p", centry, content, bitmap));

	centry->convert = convert;

	/* set bitmap entry if one is passed, free extant one if present */
	if (bitmap != NULL) {
		if (centry->bitmap != NULL) {
			bitmap_destroy(centry->bitmap);
		} else {
			image_cache_stats_bitmap_add(centry);
		}
		centry->bitmap = bitmap;
	} else {
		/* no bitmap, check to see if we should speculatively convert */
		if ((centry->convert != NULL) &&
		    (image_cache_speculate(content) == true)) {
			centry->bitmap = centry->convert(centry->content);

			if (centry->bitmap != NULL) {
				image_cache_stats_bitmap_add(centry);
			} else {
				image_cache->fail_count++;
			}
		}
	}



	return NSERROR_OK;
}

/* exported interface documented in image_cache.h */
nserror image_cache_remove(struct content *content)
{
	struct image_cache_entry_s *centry;

	/* get the cache entry */
	centry = image_cache__find(content);
	if (centry == NULL) {
		LOG(("Could not find cache entry for content (%p)", content));
		return NSERROR_NOT_FOUND;
	}

	image_cache__free_entry(centry);

	return NSERROR_OK;
}

/* exported interface documented in image_cache.h */
int image_cache_snsummaryf(char *string, size_t size, const char *fmt)
{
	size_t slen = 0; /* current output string length */
	int fmtc = 0; /* current index into format string */
	bool pct;
	unsigned int op_count;
	uint64_t op_size;

	op_count = image_cache->hit_count +
		image_cache->miss_count +
		image_cache->fail_count;

	op_size = image_cache->hit_size +
		image_cache->miss_size +
		image_cache->fail_size;

	while((slen < size) && (fmt[fmtc] != 0)) {
		if (fmt[fmtc] == '%') {
			fmtc++;

			/* check for percentage modifier */
			if (fmt[fmtc] == 'p') {
				fmtc++;
				pct = true;
			} else {
				pct = false;
			}

#define FMTCHR(chr,fmt,var) case chr : \
slen += snprintf(string + slen, size - slen, "%"fmt, image_cache->var); break

#define FMTPCHR(chr,fmt,var,div) \
case chr :					\
	if (pct) {							\
		if (div > 0) {						\
			slen += snprintf(string + slen, size - slen, "%"PRId64, (uint64_t)((image_cache->var * 100) / div)); \
		} else {						\
			slen += snprintf(string + slen, size - slen, "100"); \
		}							\
	} else {							\
		slen += snprintf(string + slen, size - slen, "%"fmt, image_cache->var); \
	} break


			switch (fmt[fmtc]) {
			case '%':
				string[slen] = '%';
				slen++;
				break;

			FMTCHR('a', SSIZET_FMT, params.limit);
			FMTCHR('b', SSIZET_FMT, params.hysteresis);
			FMTCHR('c', SSIZET_FMT, total_bitmap_size);
			FMTCHR('d', "d", bitmap_count);
			FMTCHR('e', "d", current_age / 1000);
			FMTCHR('f', SSIZET_FMT, max_bitmap_size);
			FMTCHR('g', "d", max_bitmap_size_count);
			FMTCHR('h', "d", max_bitmap_count);
			FMTCHR('i', SSIZET_FMT, max_bitmap_count_size);


			case 'j':
				slen += snprintf(string + slen, size - slen,
						 "%d", pct?100:op_count);
				break;

			FMTPCHR('k', "d", hit_count, op_count);
			FMTPCHR('l', "d", miss_count, op_count);
			FMTPCHR('m', "d", fail_count, op_count);

			case 'n':
				slen += snprintf(string + slen, size - slen,
						 "%"PRId64, pct?100:op_size);
				break;

			FMTPCHR('o', PRId64, hit_size, op_size);
			FMTPCHR('q', PRId64, miss_size, op_size);
			FMTPCHR('r', PRId64, fail_size, op_size);

			FMTCHR('s', "d", total_unrendered);
			FMTCHR('t', "d", specultive_miss_count);
			FMTCHR('u', "d", total_extra_conversions);
			FMTCHR('v', "d", total_extra_conversions_count);
			FMTCHR('w', "d", peak_conversions_size);
			FMTCHR('x', "d", peak_conversions);


			}
#undef FMTCHR
#undef FMTPCHR

			fmtc++;
		} else {
			string[slen] = fmt[fmtc];
			slen++;
			fmtc++;
		}
	}

	/* Ensure that we NUL-terminate the output */
	string[min(slen, size - 1)] = '\0';

	return slen;
}

/* exported interface documented in image_cache.h */
int image_cache_snentryf(char *string, size_t size, unsigned int entryn,
		const char *fmt)
{
	struct image_cache_entry_s *centry;
	size_t slen = 0; /* current output string length */
	int fmtc = 0; /* current index into format string */
	lwc_string *origin; /* current entry's origin */

	centry = image_cache__findn(entryn);
	if (centry == NULL)
		return -1;

	while((slen < size) && (fmt[fmtc] != 0)) {
		if (fmt[fmtc] == '%') {
			fmtc++;
			switch (fmt[fmtc]) {
			case 'e':
				slen += snprintf(string + slen, size - slen,
						"%d", entryn);
				break;

			case 'r':
				slen += snprintf(string + slen, size - slen,
						"%u", centry->redraw_count);
				break;

			case 'a':
				slen += snprintf(string + slen, size - slen,
						 "%.2f", (float)((image_cache->current_age -  centry->redraw_age)) / 1000);
				break;


			case 'c':
				slen += snprintf(string + slen, size - slen,
						"%d", centry->conversion_count);
				break;

			case 'g':
				slen += snprintf(string + slen, size - slen,
						"%.2f", (float)((image_cache->current_age -  centry->bitmap_age)) / 1000);
				break;

			case 'k':
				slen += snprintf(string + slen, size - slen,
						"%p", centry->content);
				break;

			case 'U':
				slen += snprintf(string + slen, size - slen,
				    		"%s", nsurl_access(llcache_handle_get_url(centry->content->llcache)));
				break;

			case 'o':
				if (nsurl_has_component(llcache_handle_get_url(
						centry->content->llcache),
						NSURL_HOST)) {
					origin = nsurl_get_component(
							llcache_handle_get_url(
							centry->content->
								llcache),
							NSURL_HOST);
					
					slen += snprintf(string + slen,
							size - slen, "%s",
				    			lwc_string_data(
				    				origin));

					lwc_string_unref(origin);
				} else {
					slen += snprintf(string + slen,
							size - slen, "%s",
				    			"localhost");
				}
				break;
			
			case 's':
				if (centry->bitmap != NULL) {
					slen += snprintf(string + slen,
							 size - slen,
							 "%"SSIZET_FMT,
							 centry->bitmap_size);
				} else {
					slen += snprintf(string + slen,
							 size - slen,
							 "0");
				}
				break;
			}
			fmtc++;
		} else {
			string[slen] = fmt[fmtc];
			slen++;
			fmtc++;
		}
	}

	/* Ensure that we NUL-terminate the output */
	string[min(slen, size - 1)] = '\0';

	return slen;
}


/* exported interface documented in image_cache.h */
bool image_cache_redraw(struct content *c,
			struct content_redraw_data *data,
			const struct rect *clip,
			const struct redraw_context *ctx)
{
	struct image_cache_entry_s *centry;

	/* get the cache entry */
	centry = image_cache__find(c);
	if (centry == NULL) {
		LOG(("Could not find cache entry for content (%p)", c));
		return false;
	}

	if (centry->bitmap == NULL) {
		if (centry->convert != NULL) {
			centry->bitmap = centry->convert(centry->content);
		}

		if (centry->bitmap != NULL) {
			image_cache_stats_bitmap_add(centry);
			image_cache->miss_count++;
			image_cache->miss_size += centry->bitmap_size;
		} else {
			image_cache->fail_count++;
			image_cache->fail_size += centry->bitmap_size;
			return false;
		}
	} else {
		image_cache->hit_count++;
		image_cache->hit_size += centry->bitmap_size;
	}


	/* update statistics */
	centry->redraw_count++;
	centry->redraw_age = image_cache->current_age;

	return image_bitmap_plot(centry->bitmap, data, clip, ctx);
}

void image_cache_destroy(struct content *content)
{
	struct image_cache_entry_s *centry;

	/* get the cache entry */
	centry = image_cache__find(content);
	if (centry == NULL) {
		LOG(("Could not find cache entry for content (%p)", content));
	} else {
		image_cache__free_entry(centry);
	}
}

void *image_cache_get_internal(const struct content *c, void *context)
{
	return image_cache_get_bitmap(c);
}

content_type image_cache_content_type(void)
{
	return CONTENT_IMAGE;
}
