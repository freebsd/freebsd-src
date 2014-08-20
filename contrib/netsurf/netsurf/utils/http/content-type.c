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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/http.h"

#include "utils/http/generics.h"
#include "utils/http/parameter_internal.h"
#include "utils/http/primitives.h"

/* See content-type.h for documentation */
nserror http_parse_content_type(const char *header_value,
		http_content_type **result)
{
	const char *pos = header_value;
	lwc_string *type;
	lwc_string *subtype = NULL;
	http_parameter *params = NULL;
	char *mime;
	size_t mime_len;
	lwc_string *imime;
	http_content_type *ct;
	nserror error;

	/* type "/" subtype *( ";" parameter ) */

	http__skip_LWS(&pos);

	error = http__parse_token(&pos, &type);
	if (error != NSERROR_OK)
		return error;

	http__skip_LWS(&pos);

	if (*pos != '/') {
		lwc_string_unref(type);
		return NSERROR_NOT_FOUND;
	}

	pos++;

	http__skip_LWS(&pos);

	error = http__parse_token(&pos, &subtype);
	if (error != NSERROR_OK) {
		lwc_string_unref(type);
		return error;
	}

	http__skip_LWS(&pos);

	if (*pos == ';') {
		error = http__item_list_parse(&pos, 
				http__parse_parameter, NULL, &params);
		if (error != NSERROR_OK && error != NSERROR_NOT_FOUND) {
			lwc_string_unref(subtype);
			lwc_string_unref(type);
			return error;
		}
	}

	/* <type> + <subtype> + '/' */
	mime_len = lwc_string_length(type) + lwc_string_length(subtype) + 1;

	mime = malloc(mime_len + 1);
	if (mime == NULL) {
		http_parameter_list_destroy(params);
		lwc_string_unref(subtype);
		lwc_string_unref(type);
		return NSERROR_NOMEM;
	}

	sprintf(mime, "%.*s/%.*s", 
		(int) lwc_string_length(type), lwc_string_data(type), 
		(int) lwc_string_length(subtype), lwc_string_data(subtype));

	lwc_string_unref(subtype);
	lwc_string_unref(type);

	if (lwc_intern_string(mime, mime_len, &imime) != lwc_error_ok) {
		http_parameter_list_destroy(params);
		free(mime);
		return NSERROR_NOMEM;
	}

	free(mime);

	ct = malloc(sizeof(*ct));
	if (ct == NULL) {
		lwc_string_unref(imime);
		http_parameter_list_destroy(params);
		return NSERROR_NOMEM;
	}

	ct->media_type = imime;
	ct->parameters = params;

	*result = ct;

	return NSERROR_OK;
}

/* See content-type.h for documentation */
void http_content_type_destroy(http_content_type *victim)
{
	lwc_string_unref(victim->media_type);
	http_parameter_list_destroy(victim->parameters);
	free(victim);
}

