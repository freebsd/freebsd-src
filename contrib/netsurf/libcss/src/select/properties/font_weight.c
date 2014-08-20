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

css_error css__cascade_font_weight(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_FONT_WEIGHT_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case FONT_WEIGHT_NORMAL:
			value = CSS_FONT_WEIGHT_NORMAL;
			break;
		case FONT_WEIGHT_BOLD:
			value = CSS_FONT_WEIGHT_BOLD;
			break;
		case FONT_WEIGHT_BOLDER:
			value = CSS_FONT_WEIGHT_BOLDER;
			break;
		case FONT_WEIGHT_LIGHTER:
			value = CSS_FONT_WEIGHT_LIGHTER;
			break;
		case FONT_WEIGHT_100:
			value = CSS_FONT_WEIGHT_100;
			break;
		case FONT_WEIGHT_200:
			value = CSS_FONT_WEIGHT_200;
			break;
		case FONT_WEIGHT_300:
			value = CSS_FONT_WEIGHT_300;
			break;
		case FONT_WEIGHT_400:
			value = CSS_FONT_WEIGHT_400;
			break;
		case FONT_WEIGHT_500:
			value = CSS_FONT_WEIGHT_500;
			break;
		case FONT_WEIGHT_600:
			value = CSS_FONT_WEIGHT_600;
			break;
		case FONT_WEIGHT_700:
			value = CSS_FONT_WEIGHT_700;
			break;
		case FONT_WEIGHT_800:
			value = CSS_FONT_WEIGHT_800;
			break;
		case FONT_WEIGHT_900:
			value = CSS_FONT_WEIGHT_900;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_font_weight(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_font_weight_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_font_weight(style, hint->status);
}

css_error css__initial_font_weight(css_select_state *state)
{
	return set_font_weight(state->computed, CSS_FONT_WEIGHT_NORMAL);
}

css_error css__compose_font_weight(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_font_weight(child);

	if (type == CSS_FONT_WEIGHT_INHERIT) {
		type = get_font_weight(parent);
	}

	return set_font_weight(result, type);
}

