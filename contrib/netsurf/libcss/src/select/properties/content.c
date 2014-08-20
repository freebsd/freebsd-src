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

css_error css__cascade_content(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_CONTENT_INHERIT;
	css_computed_content_item *content = NULL;
	uint32_t n_contents = 0;

	if (isInherit(opv) == false) {
		uint32_t v = getValue(opv);

		if (v == CONTENT_NORMAL) {
			value = CSS_CONTENT_NORMAL;
		} else if (v == CONTENT_NONE) {
			value = CSS_CONTENT_NONE;
		} else {
			value = CSS_CONTENT_SET;
			
			while (v != CONTENT_NORMAL) {
				lwc_string *he;
				css_computed_content_item *temp;

				css__stylesheet_string_get(style->sheet,
					*((css_code_t *) style->bytecode), &he);
				
				temp = realloc(content,
						(n_contents + 1) *
						sizeof(css_computed_content_item));
				if (temp == NULL) {
					if (content != NULL) {
						free(content);
					}
					return CSS_NOMEM;
				}

				content = temp;

				switch (v & 0xff) {
				case CONTENT_COUNTER:
					advance_bytecode(style, sizeof(css_code_t));

					content[n_contents].type =
						CSS_COMPUTED_CONTENT_COUNTER;
					content[n_contents].data.counter.name = he;
					content[n_contents].data.counter.style = v >> CONTENT_COUNTER_STYLE_SHIFT;
					break;
				case CONTENT_COUNTERS:
				{
					lwc_string *sep;
	
					advance_bytecode(style, sizeof(css_code_t));

					css__stylesheet_string_get(style->sheet, *((css_code_t *) style->bytecode), &sep);
					advance_bytecode(style, sizeof(css_code_t));

					content[n_contents].type =
						CSS_COMPUTED_CONTENT_COUNTERS;
					content[n_contents].data.counters.name = he;
					content[n_contents].data.counters.sep = sep;
					content[n_contents].data.counters.style = v >> CONTENT_COUNTERS_STYLE_SHIFT;
				}
					break;
				case CONTENT_URI:
					advance_bytecode(style, sizeof(css_code_t));

					content[n_contents].type =
						CSS_COMPUTED_CONTENT_URI;
					content[n_contents].data.uri = he;
					break;
				case CONTENT_ATTR:
					advance_bytecode(style, sizeof(css_code_t));

					content[n_contents].type =
						CSS_COMPUTED_CONTENT_ATTR;
					content[n_contents].data.attr = he;
					break;
				case CONTENT_STRING:
					advance_bytecode(style, sizeof(css_code_t));

					content[n_contents].type =
						CSS_COMPUTED_CONTENT_STRING;
					content[n_contents].data.string = he;
					break;
				case CONTENT_OPEN_QUOTE:
					content[n_contents].type =
						CSS_COMPUTED_CONTENT_OPEN_QUOTE;
					break;
				case CONTENT_CLOSE_QUOTE:
					content[n_contents].type =
						CSS_COMPUTED_CONTENT_CLOSE_QUOTE;
					break;
				case CONTENT_NO_OPEN_QUOTE:
					content[n_contents].type =
						CSS_COMPUTED_CONTENT_NO_OPEN_QUOTE;
					break;
				case CONTENT_NO_CLOSE_QUOTE:
					content[n_contents].type =
						CSS_COMPUTED_CONTENT_NO_CLOSE_QUOTE;
					break;
				}

				n_contents++;

				v = *((uint32_t *) style->bytecode);
				advance_bytecode(style, sizeof(v));
			}
		}
	}

	/* If we have some content, terminate the array with a blank entry */
	if (n_contents > 0) {
		css_computed_content_item *temp;

		temp = realloc(content, (n_contents + 1) *
				sizeof(css_computed_content_item));
		if (temp == NULL) {
			free(content);
			return CSS_NOMEM;
		}

		content = temp;

		content[n_contents].type = CSS_COMPUTED_CONTENT_NONE;
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		css_error error;

		error = set_content(state->computed, value, content);
		if (error != CSS_OK && content != NULL)
			free(content);

		return error;
	} else if (content != NULL) {
		free(content);
	}

	return CSS_OK;
}

css_error css__set_content_from_hint(const css_hint *hint, 
		css_computed_style *style)
{
	css_computed_content_item *item;
	css_error error;

	error = set_content(style, hint->status, hint->data.content);

	for (item = hint->data.content; item != NULL &&
			item->type != CSS_COMPUTED_CONTENT_NONE;
			item++) {
		switch (item->type) {
		case CSS_COMPUTED_CONTENT_STRING:
			lwc_string_unref(item->data.string);
			break;
		case CSS_COMPUTED_CONTENT_URI:
			lwc_string_unref(item->data.uri);
			break;
		case CSS_COMPUTED_CONTENT_COUNTER:
			lwc_string_unref(item->data.counter.name);
			break;
		case CSS_COMPUTED_CONTENT_COUNTERS:
			lwc_string_unref(item->data.counters.name);
			lwc_string_unref(item->data.counters.sep);
			break;
		case CSS_COMPUTED_CONTENT_ATTR:
			lwc_string_unref(item->data.attr);
			break;
		default:
			break;
		}
	}

	if (error != CSS_OK && hint->data.content != NULL)
		free(hint->data.content);

	return error;
}

css_error css__initial_content(css_select_state *state)
{
	return set_content(state->computed, CSS_CONTENT_NORMAL, NULL);
}

css_error css__compose_content(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_error error;
	const css_computed_content_item *items = NULL;
	uint8_t type = get_content(child, &items);

	if ((child->uncommon == NULL && parent->uncommon != NULL) ||
			type == CSS_CONTENT_INHERIT ||
			(child->uncommon != NULL && result != child)) {
		size_t n_items = 0;
		css_computed_content_item *copy = NULL;

		if ((child->uncommon == NULL && parent->uncommon != NULL) ||
				type == CSS_CONTENT_INHERIT) {
			type = get_content(parent, &items);
		}

		if (type == CSS_CONTENT_SET) {
			const css_computed_content_item *i;

			for (i = items; i->type != CSS_COMPUTED_CONTENT_NONE; 
					i++)
				n_items++;

			copy = malloc((n_items + 1) * 
					sizeof(css_computed_content_item));
			if (copy == NULL)
				return CSS_NOMEM;

			memcpy(copy, items, (n_items + 1) * 
					sizeof(css_computed_content_item));
		}

		error = set_content(result, type, copy);
		if (error != CSS_OK && copy != NULL)
			free(copy);

		return error;
	}

	return CSS_OK;
}

