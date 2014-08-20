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

css_error css__cascade_float(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_FLOAT_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case FLOAT_LEFT:
			value = CSS_FLOAT_LEFT;
			break;
		case FLOAT_RIGHT:
			value = CSS_FLOAT_RIGHT;
			break;
		case FLOAT_NONE:
			value = CSS_FLOAT_NONE;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_float(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_float_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_float(style, hint->status);
}

css_error css__initial_float(css_select_state *state)
{
	return set_float(state->computed, CSS_FLOAT_NONE);
}

css_error css__compose_float(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_float(child);

	if (type == CSS_FLOAT_INHERIT) {
		type = get_float(parent);
	}

	return set_float(result, type);
}

