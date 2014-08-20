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

css_error css__cascade_white_space(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_WHITE_SPACE_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case WHITE_SPACE_NORMAL:
			value = CSS_WHITE_SPACE_NORMAL;
			break;
		case WHITE_SPACE_PRE:
			value = CSS_WHITE_SPACE_PRE;
			break;
		case WHITE_SPACE_NOWRAP:
			value = CSS_WHITE_SPACE_NOWRAP;
			break;
		case WHITE_SPACE_PRE_WRAP:
			value = CSS_WHITE_SPACE_PRE_WRAP;
			break;
		case WHITE_SPACE_PRE_LINE:
			value = CSS_WHITE_SPACE_PRE_LINE;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_white_space(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_white_space_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_white_space(style, hint->status);
}

css_error css__initial_white_space(css_select_state *state)
{
	return set_white_space(state->computed, CSS_WHITE_SPACE_NORMAL);
}

css_error css__compose_white_space(const css_computed_style *parent,	
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_white_space(child);

	if (type == CSS_WHITE_SPACE_INHERIT) {
		type = get_white_space(parent);
	}

	return set_white_space(result, type);
}

