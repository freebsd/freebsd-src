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

css_error css__cascade_quotes(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_QUOTES_INHERIT;
	lwc_string **quotes = NULL;
	uint32_t n_quotes = 0;

	if (isInherit(opv) == false) {
		uint32_t v = getValue(opv);

		value = CSS_QUOTES_STRING;

		while (v != QUOTES_NONE) {
			lwc_string *open, *close;
			lwc_string **temp;

			css__stylesheet_string_get(style->sheet,
					*((css_code_t *) style->bytecode),
					&open);
			advance_bytecode(style, sizeof(css_code_t));

			css__stylesheet_string_get(style->sheet,
					*((css_code_t *) style->bytecode),
					&close);
			advance_bytecode(style, sizeof(css_code_t));

			temp = realloc(quotes, 
					(n_quotes + 2) * sizeof(lwc_string *));
			if (temp == NULL) {
				if (quotes != NULL) {
					free(quotes);
				}
				return CSS_NOMEM;
			}

			quotes = temp;

			quotes[n_quotes++] = open;
			quotes[n_quotes++] = close;

			v = *((uint32_t *) style->bytecode);
			advance_bytecode(style, sizeof(v));
		}
	}

	/* Terminate array, if required */
	if (n_quotes > 0) {
		lwc_string **temp;

		temp = realloc(quotes, (n_quotes + 1) * sizeof(lwc_string *));
		if (temp == NULL) {
			free(quotes);
			return CSS_NOMEM;
		}

		quotes = temp;

		quotes[n_quotes] = NULL;
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		css_error error;

		error = set_quotes(state->computed, value, quotes);
		if (error != CSS_OK && quotes != NULL)
			free(quotes);

		return error;
	} else {
		if (quotes != NULL)
			free(quotes);
	}

	return CSS_OK;
}

css_error css__set_quotes_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	lwc_string **item;
	css_error error;
		
	error = set_quotes(style, hint->status, hint->data.strings);

	for (item = hint->data.strings;
			item != NULL && (*item) != NULL; item++) {
		lwc_string_unref(*item);
	}

	if (error != CSS_OK && hint->data.strings != NULL)
		free(hint->data.strings);

	return error;
}

css_error css__initial_quotes(css_select_state *state)
{
	css_hint hint;
	css_error error;

	error = state->handler->ua_default_for_property(state->pw,
			CSS_PROP_QUOTES, &hint);
	if (error != CSS_OK)
		return error;

	return css__set_quotes_from_hint(&hint, state->computed);
}

css_error css__compose_quotes(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_error error;
	lwc_string **quotes = NULL;
	uint8_t type = get_quotes(child, &quotes);

	if (type == CSS_QUOTES_INHERIT || result != child) {
		size_t n_quotes = 0;
		lwc_string **copy = NULL;

		if (type == CSS_QUOTES_INHERIT) {
			type = get_quotes(parent, &quotes);
		}

		if (quotes != NULL) {
			lwc_string **i;

			for (i = quotes; (*i) != NULL; i++)
				n_quotes++;

			copy = malloc((n_quotes + 1) * sizeof(lwc_string *));
			if (copy == NULL)
				return CSS_NOMEM;

			memcpy(copy, quotes, (n_quotes + 1) * 
					sizeof(lwc_string *));
		}

		error = set_quotes(result, type, copy);
		if (error != CSS_OK && copy != NULL)
			free(copy);

		return error;
	}

	return CSS_OK;
}

