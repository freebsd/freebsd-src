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

css_error css__cascade_unicode_bidi(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_UNICODE_BIDI_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case UNICODE_BIDI_NORMAL:
			value = CSS_UNICODE_BIDI_NORMAL;
			break;
		case UNICODE_BIDI_EMBED:
			value = CSS_UNICODE_BIDI_EMBED;
			break;
		case UNICODE_BIDI_BIDI_OVERRIDE:
			value = CSS_UNICODE_BIDI_BIDI_OVERRIDE;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_unicode_bidi(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_unicode_bidi_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_unicode_bidi(style, hint->status);
}

css_error css__initial_unicode_bidi(css_select_state *state)
{
	return set_unicode_bidi(state->computed, CSS_UNICODE_BIDI_NORMAL);
}

css_error css__compose_unicode_bidi(const css_computed_style *parent,	
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_unicode_bidi(child);

	if (type == CSS_UNICODE_BIDI_INHERIT) {
		type = get_unicode_bidi(parent);
	}

	return set_unicode_bidi(result, type);
}

