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

css_error css__cascade_direction(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_DIRECTION_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case DIRECTION_LTR:
			value = CSS_DIRECTION_LTR;
			break;
		case DIRECTION_RTL:
			value = CSS_DIRECTION_RTL;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_direction(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_direction_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_direction(style, hint->status);
}

css_error css__initial_direction(css_select_state *state)
{
	return set_direction(state->computed, CSS_DIRECTION_LTR);
}

css_error css__compose_direction(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_direction(child);

	if (type == CSS_DIRECTION_INHERIT) {
		type = get_direction(parent);
	}

	return set_direction(result, type);
}

