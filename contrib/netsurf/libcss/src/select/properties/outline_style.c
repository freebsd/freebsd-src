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

css_error css__cascade_outline_style(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_border_style(opv, style, state, set_outline_style);
}

css_error css__set_outline_style_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_outline_style(style, hint->status);
}

css_error css__initial_outline_style(css_select_state *state)
{
	return set_outline_style(state->computed, CSS_OUTLINE_STYLE_NONE);
}

css_error css__compose_outline_style(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_outline_style(child);

	if (type == CSS_OUTLINE_STYLE_INHERIT) {
		type = get_outline_style(parent);
	}

	return set_outline_style(result, type);
}

