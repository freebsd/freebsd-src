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

css_error css__cascade_vertical_align(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_VERTICAL_ALIGN_INHERIT;
	css_fixed length = 0;
	uint32_t unit = UNIT_PX;

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case VERTICAL_ALIGN_SET:
			value = CSS_VERTICAL_ALIGN_SET;

			length = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(length));
			unit = *((uint32_t *) style->bytecode);
			advance_bytecode(style, sizeof(unit));
			break;
		case VERTICAL_ALIGN_BASELINE:
			value = CSS_VERTICAL_ALIGN_BASELINE;
			break;
		case VERTICAL_ALIGN_SUB:
			value = CSS_VERTICAL_ALIGN_SUB;
			break;
		case VERTICAL_ALIGN_SUPER:
			value = CSS_VERTICAL_ALIGN_SUPER;
			break;
		case VERTICAL_ALIGN_TOP:
			value = CSS_VERTICAL_ALIGN_TOP;
			break;
		case VERTICAL_ALIGN_TEXT_TOP:
			value = CSS_VERTICAL_ALIGN_TEXT_TOP;
			break;
		case VERTICAL_ALIGN_MIDDLE:
			value = CSS_VERTICAL_ALIGN_MIDDLE;
			break;
		case VERTICAL_ALIGN_BOTTOM:
			value = CSS_VERTICAL_ALIGN_BOTTOM;
			break;
		case VERTICAL_ALIGN_TEXT_BOTTOM:
			value = CSS_VERTICAL_ALIGN_TEXT_BOTTOM;
			break;
		}
	}

	unit = css__to_css_unit(unit);

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_vertical_align(state->computed, value, length, unit);
	}

	return CSS_OK;
}

css_error css__set_vertical_align_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_vertical_align(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_vertical_align(css_select_state *state)
{
	return set_vertical_align(state->computed, CSS_VERTICAL_ALIGN_BASELINE,
			0, CSS_UNIT_PX);
}

css_error css__compose_vertical_align(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_vertical_align(child, &length, &unit);

	if (type == CSS_VERTICAL_ALIGN_INHERIT) {
		type = get_vertical_align(parent, &length, &unit);
	}

	return set_vertical_align(result, type, length, unit);
}

