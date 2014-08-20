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

css_error css__cascade_line_height(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_LINE_HEIGHT_INHERIT;
	css_fixed val = 0;
	uint32_t unit = UNIT_PX;

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case LINE_HEIGHT_NUMBER:
			value = CSS_LINE_HEIGHT_NUMBER;
			val = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(val));
			break;
		case LINE_HEIGHT_DIMENSION:
			value = CSS_LINE_HEIGHT_DIMENSION;
			val = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(val));
			unit = *((uint32_t *) style->bytecode);
			advance_bytecode(style, sizeof(unit));
			break;
		case LINE_HEIGHT_NORMAL:
			value = CSS_LINE_HEIGHT_NORMAL;
			break;
		}
	}

	unit = css__to_css_unit(unit);

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_line_height(state->computed, value, val, unit);
	}

	return CSS_OK;
}

css_error css__set_line_height_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_line_height(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_line_height(css_select_state *state)
{
	return set_line_height(state->computed, CSS_LINE_HEIGHT_NORMAL, 
			0, CSS_UNIT_PX);
}

css_error css__compose_line_height(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_line_height(child, &length, &unit);

	if (type == CSS_LINE_HEIGHT_INHERIT) {
		type = get_line_height(parent, &length, &unit);
	}

	return set_line_height(result, type, length, unit);
}

