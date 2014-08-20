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

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utils/http/primitives.h"

/**
 * Skip past linear whitespace in input
 *
 * \param input  Pointer to current input byte. Updated on exit.
 */
void http__skip_LWS(const char **input)
{
	const char *pos = *input;

	while (*pos == ' ' || *pos == '\t')
		pos++;

	*input = pos;
}

/**
 * Determine if a character is valid for an HTTP token
 *
 * \param c  Character to consider
 * \return True if character is valid, false otherwise
 */
static bool http_is_token_char(uint8_t c)
{
	/* [ 32 - 126 ] except ()<>@,;:\"/[]?={} SP HT */

	if (c <= ' ' || 126 < c)
		return false;

	return (strchr("()<>@,;:\\\"/[]?={}", c) == NULL);
}

/**
 * Parse an HTTP token
 *
 * \param input  Pointer to current input byte. Updated on exit.
 * \param value  Pointer to location to receive on-heap token value.
 * \return NSERROR_OK on success,
 * 	   NSERROR_NOMEM on memory exhaustion,
 * 	   NSERROR_NOT_FOUND if no token could be parsed
 *
 * The returned value is owned by the caller
 */
nserror http__parse_token(const char **input, lwc_string **value)
{
	const uint8_t *start = (const uint8_t *) *input;
	const uint8_t *end;
	lwc_string *token;

	end = start;
	while (http_is_token_char(*end))
		end++;

	if (end == start)
		return NSERROR_NOT_FOUND;

	if (lwc_intern_string((const char *) start, 
			end - start, &token) != lwc_error_ok)
		return NSERROR_NOMEM;

	*value = token;
	*input = (const char *) end;

	return NSERROR_OK;
}

/**
 * Parse an HTTP quoted-string
 *
 * \param input  Pointer to current input byte. Updated on exit.
 * \param value  Pointer to location to receive on-heap string value.
 * \return NSERROR_OK on success,
 * 	   NSERROR_NOMEM on memory exhaustion,
 * 	   NSERROR_NOT_FOUND if no string could be parsed
 *
 * The returned value is owned by the caller
 */
nserror http__parse_quoted_string(const char **input, lwc_string **value)
{
	const uint8_t *start = (const uint8_t *) *input;
	const uint8_t *end;
	uint8_t c;
	lwc_string *string_value;

	/* <"> *( qdtext | quoted-pair ) <">
	 * qdtext = any TEXT except <">
	 * quoted-pair = "\" CHAR
	 * TEXT = [ HT, CR, LF, 32-126, 128-255 ]
	 * CHAR = [ 0 - 127 ]
	 *
	 * \todo TEXT may contain non 8859-1 chars encoded per RFC 2047
	 * \todo Support quoted-pairs
	 */

	if (*start != '"')
		return NSERROR_NOT_FOUND;

	end = start = start + 1;

	c = *end;
	while (c == '\t' || c == '\r' || c == '\n' || 
			c == ' ' || c == '!' ||
			('#' <= c && c <= 126) || c > 127) {
		end++;
		c = *end;
	}

	if (*end != '"')
		return NSERROR_NOT_FOUND;

	if (lwc_intern_string((const char *) start, end - start, 
			&string_value) != lwc_error_ok)
		return NSERROR_NOMEM;

	*value = string_value;

	*input = (const char *) end + 1;

	return NSERROR_OK;
}


