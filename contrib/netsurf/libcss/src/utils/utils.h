/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007-8 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_utils_h_
#define css_utils_h_

#include <libwapcaplet/libwapcaplet.h>

#include <libcss/types.h>
#include <libcss/errors.h>

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef SLEN
/* Calculate length of a string constant */
#define SLEN(s) (sizeof((s)) - 1) /* -1 for '\0' */
#endif

#ifndef UNUSED
#define UNUSED(x) ((x)=(x))
#endif

#ifndef N_ELEMENTS
#define N_ELEMENTS(x) (sizeof((x)) / sizeof((x)[0]))
#endif

css_fixed css__number_from_lwc_string(lwc_string *string, bool int_only,
		size_t *consumed);
css_fixed css__number_from_string(const uint8_t *data, size_t len,
		bool int_only, size_t *consumed);

static inline bool isDigit(uint8_t c)
{
	return '0' <= c && c <= '9';
}

static inline bool isHex(uint8_t c)
{
	return isDigit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
}

static inline uint32_t charToHex(uint8_t c)
{
	c -= '0';

	if (c > 9)
		c -= 'A' - '9' - 1;

	if (c > 15)
		c -= 'a' - 'A';

	return c;
}

static inline css_error css_error_from_lwc_error(lwc_error err)
{
        switch (err) {
        case lwc_error_ok:
                return CSS_OK;
        case lwc_error_oom:
                return CSS_NOMEM;
        case lwc_error_range:
                return CSS_BADPARM;
        default:
                break;
        }
        return CSS_INVALID;
}

#endif
