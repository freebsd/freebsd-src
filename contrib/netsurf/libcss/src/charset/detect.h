/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_charset_detect_h_
#define css_charset_detect_h_

#include <inttypes.h>

#include <libcss/errors.h>
#include <libcss/functypes.h>
#include <libcss/types.h>

#include <parserutils/errors.h>

/* Extract a charset from a chunk of data */
parserutils_error css__charset_extract(const uint8_t *data, size_t len,
		uint16_t *mibenum, uint32_t *source);

#endif

