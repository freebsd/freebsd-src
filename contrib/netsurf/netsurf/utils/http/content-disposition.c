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

#include <stdlib.h>

#include "utils/http.h"

#include "utils/http/generics.h"
#include "utils/http/parameter_internal.h"
#include "utils/http/primitives.h"

/* See content-disposition.h for documentation */
nserror http_parse_content_disposition(const char *header_value,
		http_content_disposition **result)
{
	const char *pos = header_value;
	lwc_string *mtype;
	http_parameter *params = NULL;
	http_content_disposition *cd;
	nserror error;

	/* disposition-type *( ";" parameter ) */

	http__skip_LWS(&pos);

	error = http__parse_token(&pos, &mtype);
	if (error != NSERROR_OK)
		return error;

	http__skip_LWS(&pos);

	if (*pos == ';') {
		error = http__item_list_parse(&pos, 
				http__parse_parameter, NULL, &params);
		if (error != NSERROR_OK && error != NSERROR_NOT_FOUND) {
			lwc_string_unref(mtype);
			return error;
		}
	}

	cd = malloc(sizeof(*cd));
	if (cd == NULL) {
		http_parameter_list_destroy(params);
		lwc_string_unref(mtype);
		return NSERROR_NOMEM;
	}

	cd->disposition_type = mtype;
	cd->parameters = params;

	*result = cd;

	return NSERROR_OK;
}

/* See content-disposition.h for documentation */
void http_content_disposition_destroy(http_content_disposition *victim)
{
	lwc_string_unref(victim->disposition_type);
	http_parameter_list_destroy(victim->parameters);
	free(victim);
}

