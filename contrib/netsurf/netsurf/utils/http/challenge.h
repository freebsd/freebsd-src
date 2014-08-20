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

#ifndef NETSURF_UTILS_HTTP_CHALLENGE_H_
#define NETSURF_UTILS_HTTP_CHALLENGE_H_

#include <libwapcaplet/libwapcaplet.h>

#include "utils/http/parameter.h"

typedef struct http_challenge http_challenge;

/**
 * Iterate over a challenge list
 *
 * \param cur         Pointer to current iteration position, list head to start
 * \param scheme      Pointer to location to receive challenge scheme
 * \param parameters  Pointer to location to receive challenge parameters
 * \return Pointer to next iteration position, or NULL for end of iteration
 */
const http_challenge *http_challenge_list_iterate(const http_challenge *cur,
		lwc_string **scheme, http_parameter **parameters);

/**
 * Destroy a list of HTTP challenges
 *
 * \param list  List to destroy
 */
void http_challenge_list_destroy(http_challenge *list);

#endif

