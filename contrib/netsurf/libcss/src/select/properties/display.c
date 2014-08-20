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

css_error css__cascade_display(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_DISPLAY_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case DISPLAY_INLINE:
			value = CSS_DISPLAY_INLINE;
			break;
		case DISPLAY_BLOCK:
			value = CSS_DISPLAY_BLOCK;
			break;
		case DISPLAY_LIST_ITEM:
			value = CSS_DISPLAY_LIST_ITEM;
			break;
		case DISPLAY_RUN_IN:
			value = CSS_DISPLAY_RUN_IN;
			break;
		case DISPLAY_INLINE_BLOCK:
			value = CSS_DISPLAY_INLINE_BLOCK;
			break;
		case DISPLAY_TABLE:
			value = CSS_DISPLAY_TABLE;
			break;
		case DISPLAY_INLINE_TABLE:
			value = CSS_DISPLAY_INLINE_TABLE;
			break;
		case DISPLAY_TABLE_ROW_GROUP:
			value = CSS_DISPLAY_TABLE_ROW_GROUP;
			break;
		case DISPLAY_TABLE_HEADER_GROUP:
			value = CSS_DISPLAY_TABLE_HEADER_GROUP;
			break;
		case DISPLAY_TABLE_FOOTER_GROUP:
			value = CSS_DISPLAY_TABLE_FOOTER_GROUP;
			break;
		case DISPLAY_TABLE_ROW:
			value = CSS_DISPLAY_TABLE_ROW;
			break;
		case DISPLAY_TABLE_COLUMN_GROUP:
			value = CSS_DISPLAY_TABLE_COLUMN_GROUP;
			break;
		case DISPLAY_TABLE_COLUMN:
			value = CSS_DISPLAY_TABLE_COLUMN;
			break;
		case DISPLAY_TABLE_CELL:
			value = CSS_DISPLAY_TABLE_CELL;
			break;
		case DISPLAY_TABLE_CAPTION:
			value = CSS_DISPLAY_TABLE_CAPTION;
			break;
		case DISPLAY_NONE:
			value = CSS_DISPLAY_NONE;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_display(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_display_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_display(style, hint->status);
}

css_error css__initial_display(css_select_state *state)
{
	return set_display(state->computed, CSS_DISPLAY_INLINE);
}

css_error css__compose_display(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_display(child);

	if (type == CSS_DISPLAY_INHERIT) {
		type = get_display(parent);
	}

	return set_display(result, type);
}

