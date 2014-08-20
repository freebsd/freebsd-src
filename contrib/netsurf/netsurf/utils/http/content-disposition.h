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

#ifndef NETSURF_UTILS_HTTP_CONTENT_DISPOSITION_H_
#define NETSURF_UTILS_HTTP_CONTENT_DISPOSITION_H_

#include <libwapcaplet/libwapcaplet.h>

#include "utils/http/parameter.h"

typedef struct http_content_disposition {
	lwc_string *disposition_type;
	http_parameter *parameters;
} http_content_disposition;

/**
 * Parse an HTTP Content-Disposition header value
 *
 * \param header_value  Header value to parse
 * \param result        Pointer to location to receive result
 * \return NSERROR_OK on success,
 *         NSERROR_NOMEM on memory exhaustion
 */
nserror http_parse_content_disposition(const char *header_value,
		http_content_disposition **result);

/**
 * Destroy a content disposition object
 *
 * \param victim  Object to destroy
 */
void http_content_disposition_destroy(http_content_disposition *victim);

#endif
