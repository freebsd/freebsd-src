/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

css_error css__cascade_azimuth(uint32_t opv, css_style *style,
		 css_select_state *state)
{
	css_fixed val = 0;
	uint32_t unit = UNIT_DEG;

	if (isInherit(opv) == false) {
		switch (getValue(opv) & ~AZIMUTH_BEHIND) {
		case AZIMUTH_ANGLE:
			val = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(val));
			unit = *((uint32_t *) style->bytecode);
			advance_bytecode(style, sizeof(unit));
			break;
		case AZIMUTH_LEFTWARDS:
		case AZIMUTH_RIGHTWARDS:
		case AZIMUTH_LEFT_SIDE:
		case AZIMUTH_FAR_LEFT:
		case AZIMUTH_LEFT:
		case AZIMUTH_CENTER_LEFT:
		case AZIMUTH_CENTER:
		case AZIMUTH_CENTER_RIGHT:
		case AZIMUTH_RIGHT:
		case AZIMUTH_FAR_RIGHT:
		case AZIMUTH_RIGHT_SIDE:
			/** \todo azimuth values */
			break;
		}

		/** \todo azimuth behind */
	}

	unit = css__to_css_unit(unit);

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state, 
			isInherit(opv))) {
		/** \todo set computed azimuth */
	}

	return CSS_OK;
}

css_error css__set_azimuth_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	UNUSED(hint);
	UNUSED(style);

	return CSS_OK;
}

css_error css__initial_azimuth(css_select_state *state)
{
	UNUSED(state);

	return CSS_OK;
}

css_error css__compose_azimuth(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	UNUSED(parent);
	UNUSED(child);
	UNUSED(result);

	return CSS_OK;
}

