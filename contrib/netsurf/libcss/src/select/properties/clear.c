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

css_error css__cascade_clear(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_CLEAR_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case CLEAR_NONE:
			value = CSS_CLEAR_NONE;
			break;
		case CLEAR_LEFT:
			value = CSS_CLEAR_LEFT;
			break;
		case CLEAR_RIGHT:
			value = CSS_CLEAR_RIGHT;
			break;
		case CLEAR_BOTH:
			value = CSS_CLEAR_BOTH;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_clear(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_clear_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	return set_clear(style, hint->status);
}

css_error css__initial_clear(css_select_state *state)
{
	return set_clear(state->computed, CSS_CLEAR_NONE);
}

css_error css__compose_clear(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_clear(child);

	if (type == CSS_CLEAR_INHERIT) {
		type = get_clear(parent);
	}

	return set_clear(result, type);
}

