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

css_error css__cascade_font_family(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_FONT_FAMILY_INHERIT;
	lwc_string **fonts = NULL;
	uint32_t n_fonts = 0;

	if (isInherit(opv) == false) {
		uint32_t v = getValue(opv);

		while (v != FONT_FAMILY_END) {
			lwc_string *font = NULL;
			lwc_string **temp;

			switch (v) {
			case FONT_FAMILY_STRING:
			case FONT_FAMILY_IDENT_LIST:
				css__stylesheet_string_get(style->sheet, 
					*((css_code_t *) style->bytecode), 
					&font);
				advance_bytecode(style, sizeof(css_code_t));
				break;
			case FONT_FAMILY_SERIF:
				if (value == CSS_FONT_FAMILY_INHERIT)
					value = CSS_FONT_FAMILY_SERIF;
				break;
			case FONT_FAMILY_SANS_SERIF:
				if (value == CSS_FONT_FAMILY_INHERIT)
					value = CSS_FONT_FAMILY_SANS_SERIF;
				break;
			case FONT_FAMILY_CURSIVE:
				if (value == CSS_FONT_FAMILY_INHERIT)
					value = CSS_FONT_FAMILY_CURSIVE;
				break;
			case FONT_FAMILY_FANTASY:
				if (value == CSS_FONT_FAMILY_INHERIT)
					value = CSS_FONT_FAMILY_FANTASY;
				break;
			case FONT_FAMILY_MONOSPACE:
				if (value == CSS_FONT_FAMILY_INHERIT)
					value = CSS_FONT_FAMILY_MONOSPACE;
				break;
			}

			/* Only use family-names which occur before the first
			 * generic-family. Any values which occur after the
			 * first generic-family are ignored. */
			/** \todo Do this at bytecode generation time? */
			if (value == CSS_FONT_FAMILY_INHERIT && font != NULL) {
				temp = realloc(fonts, 
					(n_fonts + 1) * sizeof(lwc_string *));
				if (temp == NULL) {
					if (fonts != NULL) {
						free(fonts);
					}
					return CSS_NOMEM;
				}

				fonts = temp;

				fonts[n_fonts] = font;

				n_fonts++;
			}

			v = *((uint32_t *) style->bytecode);
			advance_bytecode(style, sizeof(v));
		}
	}

	/* Terminate array with blank entry, if needed */
	if (n_fonts > 0) {
		lwc_string **temp;

		temp = realloc(fonts, (n_fonts + 1) * sizeof(lwc_string *));
		if (temp == NULL) {
			free(fonts);
			return CSS_NOMEM;
		}

		fonts = temp;

		fonts[n_fonts] = NULL;
		
		if (value == CSS_FONT_FAMILY_INHERIT) {
			/* The stylesheet doesn't specify a generic family,
			 * but it has specified named fonts.
			 * Fall back to the user agent's default family.
			 * We don't want to inherit, because that will 
			 * incorrectly overwrite the named fonts list too.
			 */
			css_hint hint;
			css_error error;
			
			error = state->handler->ua_default_for_property(
					state->pw, CSS_PROP_FONT_FAMILY, &hint);
			if (error == CSS_OK) {
				lwc_string **item;

				value = hint.status;
		
				for (item = hint.data.strings; 
						item != NULL && (*item) != NULL;
						item++) {
					lwc_string_unref(*item);
				}

				if (hint.data.strings != NULL) {
					free(hint.data.strings);
				}
			}

			if (value == CSS_FONT_FAMILY_INHERIT) {
				/* No sane UA default: assume sans-serif */
				value = CSS_FONT_FAMILY_SANS_SERIF;
			}
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		css_error error;

		error = set_font_family(state->computed, value, fonts);
		if (error != CSS_OK && n_fonts > 0)
			free(fonts);

		return error;
	} else {
		if (n_fonts > 0)
			free(fonts);
	}

	return CSS_OK;
}

css_error css__set_font_family_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	lwc_string **item;
	css_error error;

	error = set_font_family(style, hint->status, hint->data.strings);

	for (item = hint->data.strings; 
			item != NULL && (*item) != NULL; item++) {
		lwc_string_unref(*item);
	}

	if (error != CSS_OK && hint->data.strings != NULL)
		free(hint->data.strings);

	return error;
}

css_error css__initial_font_family(css_select_state *state)
{
	css_hint hint;
	css_error error;

	error = state->handler->ua_default_for_property(state->pw,
			CSS_PROP_FONT_FAMILY, &hint);
	if (error != CSS_OK)
		return error;

	return css__set_font_family_from_hint(&hint, state->computed);
}

css_error css__compose_font_family(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_error error;
	lwc_string **names = NULL;
	uint8_t type = get_font_family(child, &names);

	if (type == CSS_FONT_FAMILY_INHERIT || result != child) {
		size_t n_names = 0;
		lwc_string **copy = NULL;

		if (type == CSS_FONT_FAMILY_INHERIT)
			type = get_font_family(parent, &names);

		if (names != NULL) {
			lwc_string **i;

			for (i = names; (*i) != NULL; i++)
				n_names++;

			copy = malloc((n_names + 1) * sizeof(lwc_string *));
			if (copy == NULL)
				return CSS_NOMEM;

			memcpy(copy, names, (n_names + 1) * 
					sizeof(lwc_string *));
		}

		error = set_font_family(result, type, copy);
		if (error != CSS_OK && copy != NULL)
			free(copy);

		return error;
	}

	return CSS_OK;
}

