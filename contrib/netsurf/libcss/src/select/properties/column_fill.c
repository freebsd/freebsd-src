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

css_error css__cascade_column_fill(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case COLUMN_FILL_BALANCE:
		case COLUMN_FILL_AUTO:
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

css_error css__set_column_fill_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	UNUSED(hint);
	UNUSED(style);

	return CSS_OK;
}

css_error css__initial_column_fill(css_select_state *state)
{
	UNUSED(state);

	return CSS_OK;
}

css_error css__compose_column_fill(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	UNUSED(parent);
	UNUSED(child);
	UNUSED(result);

	return CSS_OK;
}

