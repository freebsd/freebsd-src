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

css_error css__cascade_bottom(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_length_auto(opv, style, state, set_bottom);
}

css_error css__set_bottom_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	return set_bottom(style, hint->status, 
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_bottom(css_select_state *state)
{
	return set_bottom(state->computed, CSS_BOTTOM_AUTO, 0, CSS_UNIT_PX);
}

css_error css__compose_bottom(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_bottom(child, &length, &unit);

	if (type == CSS_BOTTOM_INHERIT) {
		type = get_bottom(parent, &length, &unit);
	}

	return set_bottom(result, type, length, unit);
}

