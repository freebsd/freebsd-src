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
 * MIME type sniffer (interface)
 */

#ifndef NETSURF_CONTENT_MIMESNIFF_H_
#define NETSURF_CONTENT_MIMESNIFF_H_

#include <stdbool.h>

#include <libwapcaplet/libwapcaplet.h>
#include "utils/errors.h"

struct llcache_handle;

/**
 * Compute the effective MIME type for an object using the sniffing
 * algorithm described in http://mimesniff.spec.whatwg.org/
 *
 * \param handle          Source data handle to sniff
 * \param data            First data chunk, or NULL
 * \param len             Length of \a data, in bytes
 * \param sniff_allowed   Whether MIME type sniffing is allowed
 * \param image_only      Sniff image types only
 * \param effective_type  Location to receive computed type
 * \return NSERROR_OK on success,
 *         NSERROR_NEED_DATA iff \a data is NULL and data is needed
 *         NSERROR_NOT_FOUND if sniffing is prohibited and no 
 *                           Content-Type header was found
 */
nserror mimesniff_compute_effective_type(struct llcache_handle *handle,
		const uint8_t *data, size_t len, bool sniff_allowed,
		bool image_only, lwc_string **effective_type);

nserror mimesniff_init(void);
void mimesniff_fini(void);

#endif
