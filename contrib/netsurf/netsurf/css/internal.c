/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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

#include <string.h>

#include "css/internal.h"

#include "utils/nsurl.h"

/**
 * URL resolution callback for libcss
 *
 * \param pw    Resolution context
 * \param base  Base URI
 * \param rel   Relative URL
 * \param abs   Pointer to location to receive resolved URL
 * \return CSS_OK       on success,
 *         CSS_NOMEM    on memory exhaustion,
 *         CSS_INVALID  if resolution failed.
 */
css_error nscss_resolve_url(void *pw, const char *base, 
		lwc_string *rel, lwc_string **abs)
{
	lwc_error lerror;
	nserror error;
	nsurl *nsbase;
	nsurl *nsabs;

	/* Create nsurl from base */
	/* TODO: avoid this */
	error = nsurl_create(base, &nsbase);
	if (error != NSERROR_OK) {
		return error == NSERROR_NOMEM ? CSS_NOMEM : CSS_INVALID;
	}

	/* Resolve URI */
	error = nsurl_join(nsbase, lwc_string_data(rel), &nsabs);
	if (error != NSERROR_OK) {
		nsurl_unref(nsbase);
		return error == NSERROR_NOMEM ? CSS_NOMEM : CSS_INVALID;
	}

	nsurl_unref(nsbase);

	/* Intern it */
	lerror = lwc_intern_string(nsurl_access(nsabs),
			nsurl_length(nsabs), abs);
	if (lerror != lwc_error_ok) {
		*abs = NULL;
		nsurl_unref(nsabs);
		return lerror == lwc_error_oom ? CSS_NOMEM : CSS_INVALID;
	}

	nsurl_unref(nsabs);

	return CSS_OK;
}

