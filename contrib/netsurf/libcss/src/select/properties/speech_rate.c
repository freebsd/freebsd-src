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

css_error css__cascade_speech_rate(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	css_fixed rate = 0;

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case SPEECH_RATE_SET:
			rate = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(rate));
			break;
		case SPEECH_RATE_X_SLOW:
		case SPEECH_RATE_SLOW:
		case SPEECH_RATE_MEDIUM:
		case SPEECH_RATE_FAST:
		case SPEECH_RATE_X_FAST:
		case SPEECH_RATE_FASTER:
		case SPEECH_RATE_SLOWER:
			/** \todo convert to public values */
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		/** \todo speech-rate */
	}

	return CSS_OK;
}

css_error css__set_speech_rate_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	UNUSED(hint);
	UNUSED(style);

	return CSS_OK;
}

css_error css__initial_speech_rate(css_select_state *state)
{
	UNUSED(state);

	return CSS_OK;
}

css_error css__compose_speech_rate(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	UNUSED(parent);
	UNUSED(child);
	UNUSED(result);

	return CSS_OK;
}

