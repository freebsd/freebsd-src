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

css_error css__cascade_break_after(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case BREAK_AFTER_AUTO:
		case BREAK_AFTER_ALWAYS:
		case BREAK_AFTER_AVOID:
		case BREAK_AFTER_LEFT:
		case BREAK_AFTER_RIGHT:
		case BREAK_AFTER_PAGE:
		case BREAK_AFTER_COLUMN:
		case BREAK_AFTER_AVOID_PAGE:
		case BREAK_AFTER_AVOID_COLUMN:
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

css_error css__set_break_after_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	UNUSED(hint);
	UNUSED(style);

	return CSS_OK;
}

css_error css__initial_break_after(css_select_state *state)
{
	UNUSED(state);

	return CSS_OK;
}

css_error css__compose_break_after(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	UNUSED(parent);
	UNUSED(child);
	UNUSED(result);

	return CSS_OK;
}

