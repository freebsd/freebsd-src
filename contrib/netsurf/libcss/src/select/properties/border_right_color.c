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

css_error css__cascade_border_right_color(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_bg_border_color(opv, style, state, 
			set_border_right_color);
}

css_error css__set_border_right_color_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	return set_border_right_color(style, hint->status, hint->data.color);
}

css_error css__initial_border_right_color(css_select_state *state)
{
	return set_border_right_color(state->computed, 
			CSS_BORDER_COLOR_CURRENT_COLOR, 0);
}

css_error css__compose_border_right_color(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_color color;
	uint8_t type = get_border_right_color(child, &color);

	if (type == CSS_BORDER_COLOR_INHERIT) {
		type = get_border_right_color(parent, &color);
	}

	return set_border_right_color(result, type, color);
}

