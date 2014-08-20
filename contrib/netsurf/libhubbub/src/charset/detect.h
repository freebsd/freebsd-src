/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_charset_detect_h_
#define hubbub_charset_detect_h_

#include <inttypes.h>

#include <parserutils/errors.h>

/* Extract a charset from a chunk of data */
parserutils_error hubbub_charset_extract(const uint8_t *data, size_t len,
		uint16_t *mibenum, uint32_t *source);

/* Parse a Content-Type string for an encoding */
uint16_t hubbub_charset_parse_content(const uint8_t *value,
                uint32_t valuelen);

/* Fix up frequently misused character sets */
void hubbub_charset_fix_charset(uint16_t *charset);

#endif

