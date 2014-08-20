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

css_error css__cascade_font_size(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_FONT_SIZE_INHERIT;
	css_fixed size = 0;
	uint32_t unit = UNIT_PX;

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case FONT_SIZE_DIMENSION: 
			value = CSS_FONT_SIZE_DIMENSION;

			size = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(size));

			unit = *((uint32_t *) style->bytecode);
			advance_bytecode(style, sizeof(unit));
			break;
		case FONT_SIZE_XX_SMALL:
			value = CSS_FONT_SIZE_XX_SMALL;
			break;
		case FONT_SIZE_X_SMALL:
			value = CSS_FONT_SIZE_X_SMALL;
			break;
		case FONT_SIZE_SMALL:
			value = CSS_FONT_SIZE_SMALL;
			break;
		case FONT_SIZE_MEDIUM:
			value = CSS_FONT_SIZE_MEDIUM;
			break;
		case FONT_SIZE_LARGE:
			value = CSS_FONT_SIZE_LARGE;
			break;
		case FONT_SIZE_X_LARGE:
			value = CSS_FONT_SIZE_X_LARGE;
			break;
		case FONT_SIZE_XX_LARGE:
			value = CSS_FONT_SIZE_XX_LARGE;
			break;
		case FONT_SIZE_LARGER:
			value = CSS_FONT_SIZE_LARGER;
			break;
		case FONT_SIZE_SMALLER:
			value = CSS_FONT_SIZE_SMALLER;
			break;
		}
	}

	unit = css__to_css_unit(unit);

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_font_size(state->computed, value, size, unit);
	}

	return CSS_OK;
}

css_error css__set_font_size_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_font_size(style, hint->status, 
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_font_size(css_select_state *state)
{
	return set_font_size(state->computed, CSS_FONT_SIZE_MEDIUM, 
			0, CSS_UNIT_PX);
}

css_error css__compose_font_size(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed size = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_font_size(child, &size, &unit);

	if (type == CSS_FONT_SIZE_INHERIT) {
		type = get_font_size(parent, &size, &unit);
	}

	return set_font_size(result, type, size, unit);
}

