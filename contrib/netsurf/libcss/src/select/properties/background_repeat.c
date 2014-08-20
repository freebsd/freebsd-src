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

css_error css__cascade_background_repeat(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_BACKGROUND_REPEAT_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case BACKGROUND_REPEAT_NO_REPEAT:
			value = CSS_BACKGROUND_REPEAT_NO_REPEAT;
			break;
		case BACKGROUND_REPEAT_REPEAT_X:
			value = CSS_BACKGROUND_REPEAT_REPEAT_X;
			break;
		case BACKGROUND_REPEAT_REPEAT_Y:
			value = CSS_BACKGROUND_REPEAT_REPEAT_Y;
			break;
		case BACKGROUND_REPEAT_REPEAT:
			value = CSS_BACKGROUND_REPEAT_REPEAT;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_background_repeat(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_background_repeat_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	return set_background_repeat(style, hint->status);
}

css_error css__initial_background_repeat(css_select_state *state)
{
	return set_background_repeat(state->computed, 
			CSS_BACKGROUND_REPEAT_REPEAT);
}

css_error css__compose_background_repeat(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_background_repeat(child);

	if (type == CSS_BACKGROUND_REPEAT_INHERIT) {
		type = get_background_repeat(parent);
	}

	return set_background_repeat(result, type);
}

