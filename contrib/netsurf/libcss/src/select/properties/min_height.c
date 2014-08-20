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

css_error css__cascade_min_height(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_length(opv, style, state, set_min_height);
}

css_error css__set_min_height_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_min_height(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_min_height(css_select_state *state)
{
	return set_min_height(state->computed, CSS_MIN_HEIGHT_SET, 
			0, CSS_UNIT_PX);
}

css_error css__compose_min_height(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_min_height(child, &length, &unit);

	if (type == CSS_MIN_HEIGHT_INHERIT) {
		type = get_min_height(parent, &length, &unit);
	}

	return set_min_height(result, type, length, unit);
}

