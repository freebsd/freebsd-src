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

css_error css__cascade_z_index(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_Z_INDEX_INHERIT;
	css_fixed index = 0;

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case Z_INDEX_SET:
			value = CSS_Z_INDEX_SET;

			index = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(index));
			break;
		case Z_INDEX_AUTO:
			value = CSS_Z_INDEX_AUTO;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_z_index(state->computed, value, index);
	}

	return CSS_OK;
}

css_error css__set_z_index_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_z_index(style, hint->status, hint->data.integer);
}

css_error css__initial_z_index(css_select_state *state)
{
	return set_z_index(state->computed, CSS_Z_INDEX_AUTO, 0);
}

css_error css__compose_z_index(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	int32_t index = 0;
	uint8_t type = get_z_index(child, &index);

	if (type == CSS_Z_INDEX_INHERIT) {
		type = get_z_index(parent, &index);
	}

	return set_z_index(result, type, index);
}

