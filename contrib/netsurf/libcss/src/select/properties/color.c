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

css_error css__cascade_color(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	bool inherit = isInherit(opv);
	uint16_t value = CSS_COLOR_INHERIT;
	css_color color = 0;

	if (inherit == false) {
		switch (getValue(opv)) {
		case COLOR_TRANSPARENT:
			value = CSS_COLOR_COLOR;
			break;
		case COLOR_CURRENT_COLOR:
			/* color: currentColor always computes to inherit */
			value = CSS_COLOR_INHERIT;
			inherit = true;
			break;
		case COLOR_SET:
			value = CSS_COLOR_COLOR;
			color = *((css_color *) style->bytecode);
			advance_bytecode(style, sizeof(color));
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state, 
			inherit)) {
		return set_color(state->computed, value, color);
	}

	return CSS_OK;
}

css_error css__set_color_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	return set_color(style, hint->status, hint->data.color);
}

css_error css__initial_color(css_select_state *state)
{
	css_hint hint;
	css_error error;

	error = state->handler->ua_default_for_property(state->pw, 
			CSS_PROP_COLOR, &hint);
	if (error != CSS_OK)
		return error;

	return css__set_color_from_hint(&hint, state->computed);
}

css_error css__compose_color(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_color color;
	uint8_t type = get_color(child, &color);

	if (type == CSS_COLOR_INHERIT) {
		type = get_color(parent, &color);
	}

	return set_color(result, type, color);
}

