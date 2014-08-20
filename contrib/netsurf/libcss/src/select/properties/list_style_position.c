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

css_error css__cascade_list_style_position(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_LIST_STYLE_POSITION_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case LIST_STYLE_POSITION_INSIDE:
			value = CSS_LIST_STYLE_POSITION_INSIDE;
			break;
		case LIST_STYLE_POSITION_OUTSIDE:
			value = CSS_LIST_STYLE_POSITION_OUTSIDE;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_list_style_position(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_list_style_position_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_list_style_position(style, hint->status);
}

css_error css__initial_list_style_position(css_select_state *state)
{
	return set_list_style_position(state->computed, 
			CSS_LIST_STYLE_POSITION_OUTSIDE);
}

css_error css__compose_list_style_position(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_list_style_position(child);

	if (type == CSS_LIST_STYLE_POSITION_INHERIT) {
		type = get_list_style_position(parent);
	}

	return set_list_style_position(result, type);
}

