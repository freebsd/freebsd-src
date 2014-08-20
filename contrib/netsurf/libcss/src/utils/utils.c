/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007-9 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include "utils/utils.h"

css_fixed css__number_from_lwc_string(lwc_string *string,
		bool int_only, size_t *consumed)
{
	if (string == NULL || lwc_string_length(string) == 0 || 
			consumed == NULL)
		return 0;

	return css__number_from_string(
			(uint8_t *)lwc_string_data(string),
			lwc_string_length(string),
			int_only,
			consumed);
}

css_fixed css__number_from_string(const uint8_t *data, size_t len,
		bool int_only, size_t *consumed)
{
	const uint8_t *ptr = data;
	int sign = 1;
	int32_t intpart = 0;
	int32_t fracpart = 0;
	int32_t pwr = 1;

	if (data == NULL || len == 0 || consumed == NULL)
		return 0;

	/* number = [+-]? ([0-9]+ | [0-9]* '.' [0-9]+) */

	/* Extract sign, if any */
	if (ptr[0] == '-') {
		sign = -1;
		len--;
		ptr++;
	} else if (ptr[0] == '+') {
		len--;
		ptr++;
	}

	/* Ensure we have either a digit or a '.' followed by a digit */
	if (len == 0) {
		*consumed = 0;
		return 0;
	} else {
		if (ptr[0] == '.') {
			if (len == 1 || ptr[1] < '0' || '9' < ptr[1]) {
				*consumed = 0;
				return 0;
			}
		} else if (ptr[0] < '0' || '9' < ptr[0]) {
			*consumed = 0;
			return 0;
		}
	}

	/* Now extract intpart, assuming base 10 */
	while (len > 0) {
		/* Stop on first non-digit */
		if (ptr[0] < '0' || '9' < ptr[0])
			break;

		/* Prevent overflow of 'intpart'; proper clamping below */
		if (intpart < (1 << 22)) {
			intpart *= 10;
			intpart += ptr[0] - '0';
		}
		ptr++;
		len--;
	}

	/* And fracpart, again, assuming base 10 */
	if (int_only == false && len > 1 && ptr[0] == '.' && 
			('0' <= ptr[1] && ptr[1] <= '9')) {
		ptr++;
		len--;

		while (len > 0) {
			if (ptr[0] < '0' || '9' < ptr[0])
				break;

			if (pwr < 1000000) {
				pwr *= 10;
				fracpart *= 10;
				fracpart += ptr[0] - '0';
			}
			ptr++;
			len--;
		}
		fracpart = ((1 << 10) * fracpart + pwr/2) / pwr;
		if (fracpart >= (1 << 10)) {
			intpart++;
			fracpart &= (1 << 10) - 1;
		}
	}

	*consumed = ptr - data;

	if (sign > 0) {
		/* If the result is larger than we can represent,
		 * then clamp to the maximum value we can store. */
		if (intpart >= (1 << 21)) {
			intpart = (1 << 21) - 1;
			fracpart = (1 << 10) - 1;
		}
	}
	else {
		/* If the negated result is smaller than we can represent
		 * then clamp to the minimum value we can store. */
		if (intpart >= (1 << 21)) {
			intpart = -(1 << 21);
			fracpart = 0;
		}
		else {
			intpart = -intpart;
			if (fracpart) {
				fracpart = (1 << 10) - fracpart;
				intpart--;
			}
		}
	}

	return (intpart << 10) | fracpart;
}

