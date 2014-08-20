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

css_error css__cascade_volume(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	css_fixed val = 0;
	uint32_t unit = UNIT_PCT;

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case VOLUME_NUMBER:
			val = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(val));
			break;
		case VOLUME_DIMENSION:
			val = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(val));
			unit = *((uint32_t *) style->bytecode);
			advance_bytecode(style, sizeof(unit));
			break;
		case VOLUME_SILENT:
		case VOLUME_X_SOFT:
		case VOLUME_SOFT:
		case VOLUME_MEDIUM:
		case VOLUME_LOUD:
		case VOLUME_X_LOUD:
			/** \todo convert to public values */
			break;
		}
	}

	unit = css__to_css_unit(unit);

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		/** \todo volume */
	}

	return CSS_OK;
}

css_error css__set_volume_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	UNUSED(hint);
	UNUSED(style);

	return CSS_OK;
}

css_error css__initial_volume(css_select_state *state)
{
	UNUSED(state);

	return CSS_OK;
}

css_error css__compose_volume(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	UNUSED(parent);
	UNUSED(child);
	UNUSED(result);

	return CSS_OK;
}

