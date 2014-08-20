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

css_error css__cascade_caption_side(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_CAPTION_SIDE_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case CAPTION_SIDE_TOP:
			value = CSS_CAPTION_SIDE_TOP;
			break;
		case CAPTION_SIDE_BOTTOM:
			value = CSS_CAPTION_SIDE_BOTTOM;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_caption_side(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_caption_side_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	return set_caption_side(style, hint->status);
}

css_error css__initial_caption_side(css_select_state *state)
{
	return set_caption_side(state->computed, CSS_CAPTION_SIDE_TOP);
}

css_error css__compose_caption_side(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_caption_side(child);

	if (type == CSS_CAPTION_SIDE_INHERIT) {
		type = get_caption_side(parent);
	}
	
	return set_caption_side(result, type);
}

