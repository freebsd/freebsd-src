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

css_error css__cascade_text_align(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_TEXT_ALIGN_INHERIT;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case TEXT_ALIGN_LEFT:
			value = CSS_TEXT_ALIGN_LEFT;
			break;
		case TEXT_ALIGN_RIGHT:
			value = CSS_TEXT_ALIGN_RIGHT;
			break;
		case TEXT_ALIGN_CENTER:
			value = CSS_TEXT_ALIGN_CENTER;
			break;
		case TEXT_ALIGN_JUSTIFY:
			value = CSS_TEXT_ALIGN_JUSTIFY;
			break;
		case TEXT_ALIGN_LIBCSS_LEFT:
			value = CSS_TEXT_ALIGN_LIBCSS_LEFT;
			break;
		case TEXT_ALIGN_LIBCSS_CENTER:
			value = CSS_TEXT_ALIGN_LIBCSS_CENTER;
			break;
		case TEXT_ALIGN_LIBCSS_RIGHT:
			value = CSS_TEXT_ALIGN_LIBCSS_RIGHT;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_text_align(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_text_align_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_text_align(style, hint->status);
}

css_error css__initial_text_align(css_select_state *state)
{
	return set_text_align(state->computed, CSS_TEXT_ALIGN_DEFAULT);
}

css_error css__compose_text_align(const css_computed_style *parent,	
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_text_align(child);

	if (type == CSS_TEXT_ALIGN_INHERIT) {
		type = get_text_align(parent);
	} else if (type == CSS_TEXT_ALIGN_INHERIT_IF_NON_MAGIC) {
		/* This is purely for the benefit of HTML tables */
		type = get_text_align(parent);

		/* If the parent's text-align is a magical one, 
		 * then reset to the default value. Otherwise, 
		 * inherit as normal. */
		if (type == CSS_TEXT_ALIGN_LIBCSS_LEFT ||
				type == CSS_TEXT_ALIGN_LIBCSS_CENTER ||
				type == CSS_TEXT_ALIGN_LIBCSS_RIGHT)
			type = CSS_TEXT_ALIGN_DEFAULT;
	}

	return set_text_align(result, type);
}

