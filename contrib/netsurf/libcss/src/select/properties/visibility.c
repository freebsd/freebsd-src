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

css_error css__cascade_visibility(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_VISIBILITY_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case VISIBILITY_VISIBLE:
			value = CSS_VISIBILITY_VISIBLE;
			break;
		case VISIBILITY_HIDDEN:
			value = CSS_VISIBILITY_HIDDEN;
			break;
		case VISIBILITY_COLLAPSE:
			value = CSS_VISIBILITY_COLLAPSE;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_visibility(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_visibility_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_visibility(style, hint->status);
}

css_error css__initial_visibility(css_select_state *state)
{
	return set_visibility(state->computed, CSS_VISIBILITY_VISIBLE);
}

css_error css__compose_visibility(const css_computed_style *parent,	
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_visibility(child);

	if (type == CSS_VISIBILITY_INHERIT) {
		type = get_visibility(parent);
	}

	return set_visibility(result, type);
}

