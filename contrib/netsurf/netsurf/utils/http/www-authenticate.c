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

#include "utils/http/challenge_internal.h"
#include "utils/http/generics.h"
#include "utils/http/parameter_internal.h"
#include "utils/http/primitives.h"
#include "utils/http/www-authenticate.h"

/* See www-authenticate.h for documentation */
nserror http_parse_www_authenticate(const char *header_value,
		http_www_authenticate **result)
{
	const char *pos = header_value;
	http_challenge *first = NULL;
	http_challenge *list = NULL;
	http_www_authenticate *wa;
	nserror error;

	/* 1#challenge */

	http__skip_LWS(&pos);

	error = http__parse_challenge(&pos, &first);
	if (error != NSERROR_OK)
		return error;

	http__skip_LWS(&pos);

	if (*pos == ',') {
		error = http__item_list_parse(&pos,
				http__parse_challenge, first, &list);
		if (error != NSERROR_OK && error != NSERROR_NOT_FOUND)
			return error;
	} else {
		list = first;
	}

	wa = malloc(sizeof(*wa));
	if (wa == NULL) {
		http_challenge_list_destroy(list);
		return NSERROR_NOMEM;
	}

	wa->challenges = list;

	*result = wa;

	return NSERROR_OK;
}

/* See www-authenticate.h for documentation */
void http_www_authenticate_destroy(http_www_authenticate *victim)
{
	http_challenge_list_destroy(victim->challenges);
	free(victim);
}

