/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

css_error css__cascade_background_image(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_uri_none(opv, style, state, set_background_image);
}

css_error css__set_background_image_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	css_error error;

	error = set_background_image(style, hint->status, hint->data.string);

	if (hint->data.string != NULL)
		lwc_string_unref(hint->data.string);

	return error;
}

css_error css__initial_background_image(css_select_state *state)
{
	return set_background_image(state->computed, 
			CSS_BACKGROUND_IMAGE_NONE, NULL);
}

css_error css__compose_background_image(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	lwc_string *url;
	uint8_t type = get_background_image(child, &url);

	if (type == CSS_BACKGROUND_IMAGE_INHERIT) {
		type = get_background_image(parent, &url);
	}

	return set_background_image(result, type, url);
}

