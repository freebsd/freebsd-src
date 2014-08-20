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

css_error css__cascade_empty_cells(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_EMPTY_CELLS_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case EMPTY_CELLS_SHOW:
			value = CSS_EMPTY_CELLS_SHOW;
			break;
		case EMPTY_CELLS_HIDE:
			value = CSS_EMPTY_CELLS_HIDE;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_empty_cells(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_empty_cells_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_empty_cells(style, hint->status);
}

css_error css__initial_empty_cells(css_select_state *state)
{
	return set_empty_cells(state->computed, CSS_EMPTY_CELLS_SHOW);
}

css_error css__compose_empty_cells(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_empty_cells(child);

	if (type == CSS_EMPTY_CELLS_INHERIT) {
		type = get_empty_cells(parent);
	}

	return set_empty_cells(result, type);
}

