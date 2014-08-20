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

css_error css__cascade_speak(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case SPEAK_NORMAL:
		case SPEAK_NONE:
		case SPEAK_SPELL_OUT:
			/** \todo convert to public values */
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		/** \todo speak */
	}

	return CSS_OK;
}

css_error css__set_speak_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	UNUSED(hint);
	UNUSED(style);

	return CSS_OK;
}

css_error css__initial_speak(css_select_state *state)
{
	UNUSED(state);

	return CSS_OK;
}

css_error css__compose_speak(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	UNUSED(parent);
	UNUSED(child);
	UNUSED(result);

	return CSS_OK;
}

