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

css_error css__cascade_left(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_length_auto(opv, style, state, set_left);
}

css_error css__set_left_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_left(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_left(css_select_state *state)
{
	return set_left(state->computed, CSS_LEFT_AUTO, 0, CSS_UNIT_PX);
}

css_error css__compose_left(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_left(child, &length, &unit);

	if (type == CSS_LEFT_INHERIT) {
		type = get_left(parent, &length, &unit);
	}

	return set_left(result, type, length, unit);
}

