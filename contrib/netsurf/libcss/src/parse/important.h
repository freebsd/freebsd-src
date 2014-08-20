/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_css__parse_important_h_
#define css_css__parse_important_h_

#include "stylesheet.h"
#include "parse/language.h"

css_error css__parse_important(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint8_t *result);

void css__make_style_important(css_style *style);

#endif
