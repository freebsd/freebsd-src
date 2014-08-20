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

css_error css__cascade_clip(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_CLIP_INHERIT;
	css_computed_clip_rect rect = { 0, 0, 0, 0, 
			CSS_UNIT_PX, CSS_UNIT_PX, CSS_UNIT_PX, CSS_UNIT_PX,
			false, false, false, false };

	if (isInherit(opv) == false) {
		switch (getValue(opv) & CLIP_SHAPE_MASK) {
		case CLIP_SHAPE_RECT:
			if (getValue(opv) & CLIP_RECT_TOP_AUTO) {
				rect.top_auto = true;
			} else {
				rect.top = *((css_fixed *) style->bytecode);
				advance_bytecode(style, sizeof(css_fixed));
				rect.tunit = *((uint32_t *) style->bytecode);
				advance_bytecode(style, sizeof(uint32_t));
			}
			if (getValue(opv) & CLIP_RECT_RIGHT_AUTO) {
				rect.right_auto = true;
			} else {
				rect.right = *((css_fixed *) style->bytecode);
				advance_bytecode(style, sizeof(css_fixed));
				rect.runit = *((uint32_t *) style->bytecode);
				advance_bytecode(style, sizeof(uint32_t));
			}
			if (getValue(opv) & CLIP_RECT_BOTTOM_AUTO) {
				rect.bottom_auto = true;
			} else {
				rect.bottom = *((css_fixed *) style->bytecode);
				advance_bytecode(style, sizeof(css_fixed));
				rect.bunit = *((uint32_t *) style->bytecode);
				advance_bytecode(style, sizeof(uint32_t));
			}
			if (getValue(opv) & CLIP_RECT_LEFT_AUTO) {
				rect.left_auto = true;
			} else {
				rect.left = *((css_fixed *) style->bytecode);
				advance_bytecode(style, sizeof(css_fixed));
				rect.lunit = *((uint32_t *) style->bytecode);
				advance_bytecode(style, sizeof(uint32_t));
			}
			value = CSS_CLIP_RECT;
			break;
		case CLIP_AUTO:
			value = CSS_CLIP_AUTO;
			break;
		}
	}

	rect.tunit = css__to_css_unit(rect.tunit);
	rect.runit = css__to_css_unit(rect.runit);
	rect.bunit = css__to_css_unit(rect.bunit);
	rect.lunit = css__to_css_unit(rect.lunit);

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		return set_clip(state->computed, value, &rect);
	}

	return CSS_OK;
}

css_error css__set_clip_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	return set_clip(style, hint->status, hint->data.clip);
}

css_error css__initial_clip(css_select_state *state)
{
	css_computed_clip_rect rect = { 0, 0, 0, 0, 
			CSS_UNIT_PX, CSS_UNIT_PX, CSS_UNIT_PX, CSS_UNIT_PX,
			false, false, false, false };

	return set_clip(state->computed, CSS_CLIP_AUTO, &rect);
}

css_error css__compose_clip(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_computed_clip_rect rect = { 0, 0, 0, 0, 
			CSS_UNIT_PX, CSS_UNIT_PX, CSS_UNIT_PX, CSS_UNIT_PX,
			false, false, false, false };
	uint8_t type = get_clip(child, &rect);

	if ((child->uncommon == NULL && parent->uncommon != NULL) || 
			type == CSS_CLIP_INHERIT ||
			(child->uncommon != NULL && result != child)) {
		if ((child->uncommon == NULL && parent->uncommon != NULL) || 
				type == CSS_CLIP_INHERIT) {
			type = get_clip(parent, &rect);
		}

		return set_clip(result, type, &rect);
	}

	return CSS_OK;
}

