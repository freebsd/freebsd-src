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

css_error css__cascade_orphans(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_number(opv, style, state, set_orphans);
}

css_error css__set_orphans_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_orphans(style, hint->status, hint->data.integer);
}

css_error css__initial_orphans(css_select_state *state)
{
	return set_orphans(state->computed, CSS_ORPHANS_SET, 2);
}

css_error css__compose_orphans(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	int32_t count = 0;
	uint8_t type = get_orphans(child, &count);
	
	if (type == CSS_ORPHANS_INHERIT) {
		type = get_orphans(parent, &count);
	}
	
	return set_orphans(result, type, count);
}

