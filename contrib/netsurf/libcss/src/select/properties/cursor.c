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

css_error css__cascade_cursor(uint32_t opv, css_style *style, 
		css_select_state *state)
{	
	uint16_t value = CSS_CURSOR_INHERIT;
	lwc_string **uris = NULL;
	uint32_t n_uris = 0;

	if (isInherit(opv) == false) {
		uint32_t v = getValue(opv);

		while (v == CURSOR_URI) {
			lwc_string *uri;
			lwc_string **temp;

			css__stylesheet_string_get(style->sheet,
					*((css_code_t *) style->bytecode),
					&uri);
			advance_bytecode(style, sizeof(css_code_t));

			temp = realloc(uris, 
					(n_uris + 1) * sizeof(lwc_string *));
			if (temp == NULL) {
				if (uris != NULL) {
					free(uris);
				}
				return CSS_NOMEM;
			}

			uris = temp;

			uris[n_uris] = uri;

			n_uris++;

			v = *((uint32_t *) style->bytecode);
			advance_bytecode(style, sizeof(v));
		}

		switch (v) {
		case CURSOR_AUTO:
			value = CSS_CURSOR_AUTO;
			break;
		case CURSOR_CROSSHAIR:
			value = CSS_CURSOR_CROSSHAIR;
			break;
		case CURSOR_DEFAULT:
			value = CSS_CURSOR_DEFAULT;
			break;
		case CURSOR_POINTER:
			value = CSS_CURSOR_POINTER;
			break;
		case CURSOR_MOVE:
			value = CSS_CURSOR_MOVE;
			break;
		case CURSOR_E_RESIZE:
			value = CSS_CURSOR_E_RESIZE;
			break;
		case CURSOR_NE_RESIZE:
			value = CSS_CURSOR_NE_RESIZE;
			break;
		case CURSOR_NW_RESIZE:
			value = CSS_CURSOR_NW_RESIZE;
			break;
		case CURSOR_N_RESIZE:
			value = CSS_CURSOR_N_RESIZE;
			break;
		case CURSOR_SE_RESIZE:
			value = CSS_CURSOR_SE_RESIZE;
			break;
		case CURSOR_SW_RESIZE:
			value = CSS_CURSOR_SW_RESIZE;
			break;
		case CURSOR_S_RESIZE:
			value = CSS_CURSOR_S_RESIZE;
			break;
		case CURSOR_W_RESIZE:
			value = CSS_CURSOR_W_RESIZE;
			break;
		case CURSOR_TEXT:
			value = CSS_CURSOR_TEXT;
			break;
		case CURSOR_WAIT:
			value = CSS_CURSOR_WAIT;
			break;
		case CURSOR_HELP:
			value = CSS_CURSOR_HELP;
			break;
		case CURSOR_PROGRESS:
			value = CSS_CURSOR_PROGRESS;
			break;
		}
	}

	/* Terminate array with blank entry, if needed */
	if (n_uris > 0) {
		lwc_string **temp;

		temp = realloc(uris, 
				(n_uris + 1) * sizeof(lwc_string *));
		if (temp == NULL) {
			free(uris);
			return CSS_NOMEM;
		}

		uris = temp;

		uris[n_uris] = NULL;
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		css_error error;

		error = set_cursor(state->computed, value, uris);
		if (error != CSS_OK && n_uris > 0)
			free(uris);

		return error;
	} else {
		if (n_uris > 0)
			free(uris);
	}

	return CSS_OK;
}

css_error css__set_cursor_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	lwc_string **item;
	css_error error;

	error = set_cursor(style, hint->status, hint->data.strings);

	for (item = hint->data.strings; 
			item != NULL && (*item) != NULL; item++) {
		lwc_string_unref(*item);
	}

	if (error != CSS_OK && hint->data.strings != NULL)
		free(hint->data.strings);

	return error;
}

css_error css__initial_cursor(css_select_state *state)
{
	return set_cursor(state->computed, CSS_CURSOR_AUTO, NULL);
}

css_error css__compose_cursor(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_error error;
	lwc_string **urls = NULL;
	uint8_t type = get_cursor(child, &urls);

	if ((child->uncommon == NULL && parent->uncommon != NULL) ||
			type == CSS_CURSOR_INHERIT ||
			(child->uncommon != NULL && result != child)) {
		size_t n_urls = 0;
		lwc_string **copy = NULL;

		if ((child->uncommon == NULL && parent->uncommon != NULL) ||
				type == CSS_CURSOR_INHERIT) {
			type = get_cursor(parent, &urls);
		}

		if (urls != NULL) {
			lwc_string **i;

			for (i = urls; (*i) != NULL; i++)
				n_urls++;

			copy = malloc((n_urls + 1) * 
					sizeof(lwc_string *));
			if (copy == NULL)
				return CSS_NOMEM;

			memcpy(copy, urls, (n_urls + 1) * 
					sizeof(lwc_string *));
		}

		error = set_cursor(result, type, copy);
		if (error != CSS_OK && copy != NULL)
			free(copy);

		return error;
	}

	return CSS_OK;
}

