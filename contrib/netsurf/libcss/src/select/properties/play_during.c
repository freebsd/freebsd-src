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

css_error css__cascade_play_during(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	lwc_string *uri = NULL;

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case PLAY_DURING_URI:
			css__stylesheet_string_get(style->sheet, *((css_code_t *) style->bytecode), &uri);
			advance_bytecode(style, sizeof(css_code_t));
			break;
		case PLAY_DURING_AUTO:
		case PLAY_DURING_NONE:
			/** \todo convert to public values */
			break;
		}

		/** \todo mix & repeat */
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		/** \todo play-during */
	}

	return CSS_OK;
}

css_error css__set_play_during_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	UNUSED(hint);
	UNUSED(style);

	return CSS_OK;
}

css_error css__initial_play_during(css_select_state *state)
{
	UNUSED(state);

	return CSS_OK;
}

css_error css__compose_play_during(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	UNUSED(parent);
	UNUSED(child);
	UNUSED(result);

	return CSS_OK;
}

