/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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

#ifndef NETSURF_CSS_SELECT_H_
#define NETSURF_CSS_SELECT_H_

#include <stdint.h>

#include <dom/dom.h>

#include "css/css.h"
#include "utils/nsurl.h"

struct content;

/**
 * Selection context
 */
typedef struct nscss_select_ctx
{
	css_select_ctx *ctx;
	bool quirks;
	nsurl *base_url;
	lwc_string *universal;
	const css_computed_style *parent_style;
} nscss_select_ctx;

css_stylesheet *nscss_create_inline_style(const uint8_t *data, size_t len,
		const char *charset, const char *url, bool allow_quirks);

css_select_results *nscss_get_style(nscss_select_ctx *ctx, dom_node *n,
		uint64_t media, const css_stylesheet *inline_style);

css_computed_style *nscss_get_blank_style(nscss_select_ctx *ctx,
		const css_computed_style *parent);

css_error nscss_compute_font_size(void *pw, const css_hint *parent, 
		css_hint *size);

bool nscss_parse_colour(const char *data, css_color *result);

#endif
