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

css_error css__cascade_page_break_inside(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_page_break_after_before_inside(opv, style, state, 
			set_page_break_inside);
}

css_error css__set_page_break_inside_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_page_break_inside(style, hint->status);
}

css_error css__initial_page_break_inside(css_select_state *state)
{
	return set_page_break_inside(state->computed,
			CSS_PAGE_BREAK_INSIDE_AUTO);
}

css_error css__compose_page_break_inside(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_page_break_inside(child);
   
	if (type == CSS_PAGE_BREAK_INSIDE_INHERIT) {
		type = get_page_break_inside(parent);
	}
   
	return set_page_break_inside(result, type);
}

