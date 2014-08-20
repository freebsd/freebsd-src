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

#ifndef NETSURF_UTILS_HTTP_PARAMETER_H_
#define NETSURF_UTILS_HTTP_PARAMETER_H_

#include <libwapcaplet/libwapcaplet.h>

#include "utils/errors.h"

typedef struct http_parameter http_parameter;

/**
 * Find a named item in an HTTP parameter list
 *
 * \param list   List to search
 * \param name   Name of item to search for
 * \param value  Pointer to location to receive value
 * \return NSERROR_OK on success,
 *         NSERROR_NOT_FOUND if requested item does not exist
 */
nserror http_parameter_list_find_item(const http_parameter *list,
		lwc_string *name, lwc_string **value);

/**
 * Iterate over a parameter list
 *
 * \param cur    Pointer to current iteration position, list head to start
 * \param name   Pointer to location to receive item name
 * \param value  Pointer to location to receive item value
 * \return Pointer to next iteration position, or NULL for end of iteration
 */
const http_parameter *http_parameter_list_iterate(const http_parameter *cur,
		lwc_string **name, lwc_string **value);

/**
 * Destroy a list of HTTP parameters
 *
 * \param list  List to destroy
 */
void http_parameter_list_destroy(http_parameter *list);

#endif

