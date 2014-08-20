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

css_error css__cascade_background_position(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_BACKGROUND_POSITION_INHERIT;
	css_fixed hlength = 0;
	css_fixed vlength = 0;
	uint32_t hunit = UNIT_PX;
	uint32_t vunit = UNIT_PX;

	if (isInherit(opv) == false) {
		value = CSS_BACKGROUND_POSITION_SET;

		switch (getValue(opv) & 0xf0) {
		case BACKGROUND_POSITION_HORZ_SET:
			hlength = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(hlength));
			hunit = *((uint32_t *) style->bytecode);
			advance_bytecode(style, sizeof(hunit));
			break;
		case BACKGROUND_POSITION_HORZ_CENTER:
			hlength = INTTOFIX(50);
			hunit = UNIT_PCT;
			break;
		case BACKGROUND_POSITION_HORZ_RIGHT:
			hlength = INTTOFIX(100);
			hunit = UNIT_PCT;
			break;
		case BACKGROUND_POSITION_HORZ_LEFT:
			hlength = INTTOFIX(0);
			hunit = UNIT_PCT;
			break;
		}

		switch (getValue(opv) & 0x0f) {
		case BACKGROUND_POSITION_VERT_SET:
			vlength = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(vlength));
			vunit = *((uint32_t *) style->bytecode);
			advance_bytecode(style, sizeof(vunit));
			break;
		case BACKGROUND_POSITION_VERT_CENTER:
			vlength = INTTOFIX(50);
			vunit = UNIT_PCT;
			break;
		case BACKGROUND_POSITION_VERT_BOTTOM:
			vlength = INTTOFIX(100);
			vunit = UNIT_PCT;
			break;
		case BACKGROUND_POSITION_VERT_TOP:
			vlength = INTTOFIX(0);
			vunit = UNIT_PCT;
			break;
		}
	}

	hunit = css__to_css_unit(hunit);
	vunit = css__to_css_unit(vunit);

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_background_position(state->computed, value,
				hlength, hunit, vlength, vunit);
	}

	return CSS_OK;
}

css_error css__set_background_position_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	return set_background_position(style, hint->status, 
		hint->data.position.h.value, hint->data.position.h.unit,
		hint->data.position.v.value, hint->data.position.v.unit);
}

css_error css__initial_background_position(css_select_state *state)
{
	return set_background_position(state->computed, 
			CSS_BACKGROUND_POSITION_SET, 
			0, CSS_UNIT_PCT, 0, CSS_UNIT_PCT);
}

css_error css__compose_background_position(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed hlength = 0, vlength = 0;
	css_unit hunit = CSS_UNIT_PX, vunit = CSS_UNIT_PX;
	uint8_t type = get_background_position(child, &hlength, &hunit, 
			&vlength, &vunit);

	if (type == CSS_BACKGROUND_POSITION_INHERIT) {
		type = get_background_position(parent,
				&hlength, &hunit, &vlength, &vunit);
	}

	return set_background_position(result, type, hlength, hunit,
				vlength, vunit);
}

