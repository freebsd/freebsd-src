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

#include "utils/http/challenge_internal.h"
#include "utils/http/generics.h"
#include "utils/http/parameter_internal.h"
#include "utils/http/primitives.h"

/**
 * Representation of an HTTP challenge
 */
struct http_challenge {
	http__item base;

	lwc_string *scheme;		/**< Challenge scheme */
	http_parameter *params;		/**< Challenge parameters */
};

/**
 * Destroy an HTTP challenge
 *
 * \param self  Challenge to destroy
 */
static void http_destroy_challenge(http_challenge *self)
{
	lwc_string_unref(self->scheme);
	http_parameter_list_destroy(self->params);
	free(self);
}

/**
 * Parse an HTTP challenge
 *
 * \param input      Pointer to current input byte. Updated on exit.
 * \param challenge  Pointer to location to receive challenge
 * \return NSERROR_OK on success,
 * 	   NSERROR_NOMEM on memory exhaustion,
 * 	   NSERROR_NOT_FOUND if no parameter could be parsed
 *
 * The returned challenge is owned by the caller.
 */
nserror http__parse_challenge(const char **input, http_challenge **challenge)
{
	const char *pos = *input;
	http_challenge *result;
	lwc_string *scheme;
	http_parameter *first = NULL;
	http_parameter *params = NULL;
	nserror error;

	/* challenge   = auth-scheme 1*SP 1#auth-param
	 * auth-scheme = token
	 * auth-param  = parameter
	 */

	error = http__parse_token(&pos, &scheme);
	if (error != NSERROR_OK)
		return error;

	if (*pos != ' ' && *pos != '\t') {
		lwc_string_unref(scheme);
		return NSERROR_NOT_FOUND;
	}

	http__skip_LWS(&pos);

	error = http__parse_parameter(&pos, &first);
	if (error != NSERROR_OK) {
		lwc_string_unref(scheme);
		return error;
	}

	http__skip_LWS(&pos);

	if (*pos == ',') {
		error = http__item_list_parse(&pos, 
				http__parse_parameter, first, &params);
		if (error != NSERROR_OK && error != NSERROR_NOT_FOUND) {
			lwc_string_unref(scheme);
			return error;
		}
	} else {
		params = first;
	}

	result = malloc(sizeof(*result));
	if (result == NULL) {
		http_parameter_list_destroy(params);
		lwc_string_unref(scheme);
		return NSERROR_NOMEM;
	}

	HTTP__ITEM_INIT(result, NULL, http_destroy_challenge);
	result->scheme = scheme;
	result->params = params;

	*challenge = result;
	*input = pos;

	return NSERROR_OK;
}

/* See challenge.h for documentation */
const http_challenge *http_challenge_list_iterate(const http_challenge *cur,
		lwc_string **scheme, http_parameter **parameters)
{
	if (cur == NULL)
		return NULL;

	*scheme = lwc_string_ref(cur->scheme);
	*parameters = cur->params;

	return (http_challenge *) cur->base.next;
}

/* See challenge.h for documentation */
void http_challenge_list_destroy(http_challenge *list)
{
	http__item_list_destroy(list);
}

