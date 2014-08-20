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

css_error css__cascade_counter_reset(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	return css__cascade_counter_increment_reset(opv, style, state,
			set_counter_reset);
}

css_error css__set_counter_reset_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	css_computed_counter *item;
	css_error error;

	error = set_counter_reset(style, hint->status, hint->data.counter);

	if (hint->status == CSS_COUNTER_RESET_NAMED &&
			hint->data.counter != NULL) {
		for (item = hint->data.counter; item->name != NULL; item++) {
			lwc_string_unref(item->name);
		}
	}

	if (error != CSS_OK && hint->data.counter != NULL)
		free(hint->data.counter);

	return error;
}

css_error css__initial_counter_reset(css_select_state *state)
{
	return set_counter_reset(state->computed, CSS_COUNTER_RESET_NONE, NULL);
}

css_error css__compose_counter_reset(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_error error;
	const css_computed_counter *items = NULL;
	uint8_t type = get_counter_reset(child, &items);

	if ((child->uncommon == NULL && parent->uncommon != NULL) ||
			type ==	CSS_COUNTER_RESET_INHERIT ||
			(child->uncommon != NULL && result != child)) {
		size_t n_items = 0;
		css_computed_counter *copy = NULL;

		if ((child->uncommon == NULL && parent->uncommon != NULL) ||
				type ==	CSS_COUNTER_RESET_INHERIT) {
			type = get_counter_reset(parent, &items);
		}

		if (type == CSS_COUNTER_RESET_NAMED && items != NULL) {
			const css_computed_counter *i;

			for (i = items; i->name != NULL; i++)
				n_items++;

			copy = malloc((n_items + 1) * 
					sizeof(css_computed_counter));
			if (copy == NULL)
				return CSS_NOMEM;

			memcpy(copy, items, (n_items + 1) * 
					sizeof(css_computed_counter));
		}

		error = set_counter_reset(result, type, copy);
		if (error != CSS_OK && copy != NULL)
			free(copy);

		return error;
	}

	return CSS_OK;
}
