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

css_error css__cascade_letter_spacing(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_length_normal(opv, style, state, set_letter_spacing);
}

css_error css__set_letter_spacing_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_letter_spacing(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_letter_spacing(css_select_state *state)
{
	return set_letter_spacing(state->computed, CSS_LETTER_SPACING_NORMAL, 
			0, CSS_UNIT_PX);
}

css_error css__compose_letter_spacing(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_letter_spacing(child, &length, &unit);

	if ((child->uncommon == NULL && parent->uncommon != NULL) || 
			type == CSS_LETTER_SPACING_INHERIT ||
			(child->uncommon != NULL && result != child)) {
		if ((child->uncommon == NULL && parent->uncommon != NULL) || 
				type == CSS_LETTER_SPACING_INHERIT) {
			type = get_letter_spacing(parent, &length, &unit);
		}

		return set_letter_spacing(result, type, length, unit);
	}

	return CSS_OK;
}

