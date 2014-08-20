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

css_error css__cascade_voice_family(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = 0;
	lwc_string **voices = NULL;
	uint32_t n_voices = 0;

	if (isInherit(opv) == false) {
		uint32_t v = getValue(opv);

		while (v != VOICE_FAMILY_END) {
			lwc_string *voice = NULL;
			lwc_string **temp;

			switch (v) {
			case VOICE_FAMILY_STRING:
			case VOICE_FAMILY_IDENT_LIST:
				css__stylesheet_string_get(style->sheet,
					*((css_code_t *) style->bytecode),
					&voice);
				advance_bytecode(style, sizeof(css_code_t));
				break;
			case VOICE_FAMILY_MALE:
				if (value == 0)
					value = 1;
				break;
			case VOICE_FAMILY_FEMALE:
				if (value == 0)
					value = 1;
				break;
			case VOICE_FAMILY_CHILD:
				if (value == 0)
					value = 1;
				break;
			}

			/* Only use family-names which occur before the first
			 * generic-family. Any values which occur after the
			 * first generic-family are ignored. */
			/** \todo Do this at bytecode generation time? */
			if (value == 0 && voice != NULL) {
				temp = realloc(voices, 
					(n_voices + 1) * sizeof(lwc_string *));
				if (temp == NULL) {
					if (voices != NULL) {
						free(voices);
					}
					return CSS_NOMEM;
				}

				voices = temp;

				voices[n_voices] = voice;

				n_voices++;
			}

			v = *((uint32_t *) style->bytecode);
			advance_bytecode(style, sizeof(v));
		}
	}

	/* Terminate array with blank entry, if needed */
	if (n_voices > 0) {
		lwc_string **temp;

		temp = realloc(voices, (n_voices + 1) * sizeof(lwc_string *));
		if (temp == NULL) {
			free(voices);
			return CSS_NOMEM;
		}

		voices = temp;

		voices[n_voices] = NULL;
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		/** \todo voice-family */
		if (n_voices > 0)
			free(voices);
	} else {
		if (n_voices > 0)
			free(voices);
	}

	return CSS_OK;
}

css_error css__set_voice_family_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	UNUSED(hint);
	UNUSED(style);

	return CSS_OK;
}

css_error css__initial_voice_family(css_select_state *state)
{
	UNUSED(state);

	return CSS_OK;
}

css_error css__compose_voice_family(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	UNUSED(parent);
	UNUSED(child);
	UNUSED(result);

	return CSS_OK;
}

