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

css_error css__cascade_text_indent(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_length(opv, style, state, set_text_indent);
}

css_error css__set_text_indent_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_text_indent(style, hint->status,
			hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_text_indent(css_select_state *state)
{
	return set_text_indent(state->computed, CSS_TEXT_INDENT_SET, 
			0, CSS_UNIT_PX);
}

css_error css__compose_text_indent(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_PX;
	uint8_t type = get_text_indent(child, &length, &unit);

	if (type == CSS_TEXT_INDENT_INHERIT) {
		type = get_text_indent(parent, &length, &unit);
	}

	return set_text_indent(result, type, length, unit);
}

