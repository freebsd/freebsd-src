/*
 * Copyright 2010 John-Mark Bell <jmb@netsurf-browser.org>
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

#ifndef NETSURF_UTILS_HTTP_GENERICS_H_
#define NETSURF_UTILS_HTTP_GENERICS_H_

#include "utils/errors.h"

/**
 * Representation of an item
 */
typedef struct http__item {
	struct http__item *next;		/**< Next item in list, or NULL */

	void (*free)(struct http__item *self);	/**< Item destructor */
} http__item;

#define HTTP__ITEM_INIT(item, n, f) \
		((http__item *) (item))->next = (http__item *) (n); \
		((http__item *) (item))->free = (void (*)(http__item *)) (f)

/**
 * Type of an item parser
 */
typedef nserror (*http__itemparser)(const char **input, http__item **item);


void http___item_list_destroy(http__item *list);
#define http__item_list_destroy(l) \
		http___item_list_destroy((http__item *) (l))

nserror http___item_list_parse(const char **input, 
		http__itemparser itemparser, http__item *first, 
		http__item **items);
#define http__item_list_parse(i, p, f, r) \
		http___item_list_parse((i), \
				(http__itemparser) (p), \
				(http__item *) (f), \
				(http__item **) (void *) (r))

#endif
