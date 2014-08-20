/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Michael Drake <tlsa@netsurf-browser.org>
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

css_error css__cascade_break_inside(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case BREAK_INSIDE_AUTO:
		case BREAK_INSIDE_AVOID:
		case BREAK_INSIDE_AVOID_PAGE:
		case BREAK_INSIDE_AVOID_COLUMN:
			/** \todo convert to public values */
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		/** \todo set computed elevation */
	}

	return CSS_OK;
}

css_error css__set_break_inside_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	UNUSED(hint);
	UNUSED(style);

	return CSS_OK;
}

css_error css__initial_break_inside(css_select_state *state)
{
	UNUSED(state);

	return CSS_OK;
}

css_error css__compose_break_inside(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	UNUSED(parent);
	UNUSED(child);
	UNUSED(result);

	return CSS_OK;
}

