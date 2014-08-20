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

css_error css__cascade_text_transform(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_TEXT_TRANSFORM_INHERIT;
	
	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case TEXT_TRANSFORM_CAPITALIZE:
			value = CSS_TEXT_TRANSFORM_CAPITALIZE;
			break;
		case TEXT_TRANSFORM_UPPERCASE:
			value = CSS_TEXT_TRANSFORM_UPPERCASE;
			break;
		case TEXT_TRANSFORM_LOWERCASE:
			value = CSS_TEXT_TRANSFORM_LOWERCASE;
			break;
		case TEXT_TRANSFORM_NONE:
			value = CSS_TEXT_TRANSFORM_NONE;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_text_transform(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_text_transform_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_text_transform(style, hint->status);
}

css_error css__initial_text_transform(css_select_state *state)
{
	return set_text_transform(state->computed, CSS_TEXT_TRANSFORM_NONE);
}

css_error css__compose_text_transform(const css_computed_style *parent,	
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_text_transform(child);

	if (type == CSS_TEXT_TRANSFORM_INHERIT) {
		type = get_text_transform(parent);
	}

	return set_text_transform(result, type);
}

