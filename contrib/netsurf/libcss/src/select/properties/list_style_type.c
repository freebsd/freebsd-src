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

css_error css__cascade_list_style_type(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_LIST_STYLE_TYPE_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case LIST_STYLE_TYPE_DISC:
			value = CSS_LIST_STYLE_TYPE_DISC;
			break;
		case LIST_STYLE_TYPE_CIRCLE:
			value = CSS_LIST_STYLE_TYPE_CIRCLE;
			break;
		case LIST_STYLE_TYPE_SQUARE:
			value = CSS_LIST_STYLE_TYPE_SQUARE;
			break;
		case LIST_STYLE_TYPE_DECIMAL:
			value = CSS_LIST_STYLE_TYPE_DECIMAL;
			break;
		case LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO:
			value = CSS_LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO;
			break;
		case LIST_STYLE_TYPE_LOWER_ROMAN:
			value = CSS_LIST_STYLE_TYPE_LOWER_ROMAN;
			break;
		case LIST_STYLE_TYPE_UPPER_ROMAN:
			value = CSS_LIST_STYLE_TYPE_UPPER_ROMAN;
			break;
		case LIST_STYLE_TYPE_LOWER_GREEK:
			value = CSS_LIST_STYLE_TYPE_LOWER_GREEK;
			break;
		case LIST_STYLE_TYPE_LOWER_LATIN:
			value = CSS_LIST_STYLE_TYPE_LOWER_LATIN;
			break;
		case LIST_STYLE_TYPE_UPPER_LATIN:
			value = CSS_LIST_STYLE_TYPE_UPPER_LATIN;
			break;
		case LIST_STYLE_TYPE_ARMENIAN:
			value = CSS_LIST_STYLE_TYPE_ARMENIAN;
			break;
		case LIST_STYLE_TYPE_GEORGIAN:
			value = CSS_LIST_STYLE_TYPE_GEORGIAN;
			break;
		case LIST_STYLE_TYPE_LOWER_ALPHA:
			value = CSS_LIST_STYLE_TYPE_LOWER_ALPHA;
			break;
		case LIST_STYLE_TYPE_UPPER_ALPHA:
			value = CSS_LIST_STYLE_TYPE_UPPER_ALPHA;
			break;
		case LIST_STYLE_TYPE_NONE:
			value = CSS_LIST_STYLE_TYPE_NONE;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_list_style_type(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_list_style_type_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_list_style_type(style, hint->status);
}

css_error css__initial_list_style_type(css_select_state *state)
{
	return set_list_style_type(state->computed, CSS_LIST_STYLE_TYPE_DISC);
}

css_error css__compose_list_style_type(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_list_style_type(child);

	if (type == CSS_LIST_STYLE_TYPE_INHERIT) {
		type = get_list_style_type(parent);
	}

	return set_list_style_type(result, type);
}

