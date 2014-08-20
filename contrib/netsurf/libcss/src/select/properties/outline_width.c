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

css_error css__cascade_outline_width(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_border_width(opv, style, state, set_outline_width);
}

css_error css__set_outline_width_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_outline_width(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_outline_width(css_select_state *state)
{
	return set_outline_width(state->computed, CSS_OUTLINE_WIDTH_MEDIUM,
			0, CSS_UNIT_PX);
}

css_error css__compose_outline_width(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_outline_width(child, &length, &unit);

	if ((child->uncommon == NULL && parent->uncommon != NULL) ||
			type == CSS_OUTLINE_WIDTH_INHERIT ||
			(child->uncommon != NULL && result != child)) {
		if ((child->uncommon == NULL && parent->uncommon != NULL) ||
				type == CSS_OUTLINE_WIDTH_INHERIT) {
			type = get_outline_width(parent, &length, &unit);
		}

		return set_outline_width(result, type, length, unit);
	}

	return CSS_OK;
}

