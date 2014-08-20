/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include "utils/utils.h"
#include "tokeniser/entities.h"

/** Node in our entity tree */
typedef struct hubbub_entity_node {
        /* Do not reorder this without fixing make-entities.pl */
	uint8_t split;	/**< Data to split on */
	int32_t lt;	/**< Subtree for data less than split */
	int32_t eq;	/**< Subtree for data equal to split */
	int32_t gt;	/**< Subtree for data greater than split */
	uint32_t value;	/**< Data for this node */
} hubbub_entity_node;

#include "entities.inc"

/**
 * Step-wise search for a key in our entity tree
 *
 * \param c        Character to look for
 * \param result   Pointer to location for result
 * \param context  Pointer to location for search context
 * \return HUBBUB_OK if key found,
 *         HUBBUB_NEEDDATA if more steps are required
 *         HUBBUB_INVALID if nothing matches
 *
 * The value pointed to by ::context must be NULL for the first call.
 * Thereafter, pass in the same value as returned by the previous call.
 * The context is opaque to the caller and should not be inspected.
 *
 * The location pointed to by ::result will be set to NULL unless a match
 * is found.
 */
static hubbub_error hubbub_entity_tree_search_step(uint8_t c,
		uint32_t *result, int32_t *context)
{
	bool match = false;
	int32_t p;

	if (result == NULL || context == NULL)
		return HUBBUB_BADPARM;

	if (*context == -1) {
		p = dict_root;
	} else {
		p = *context;
	}

	while (p != -1) {
		if (c < dict[p].split) {
			p = dict[p].lt;
		} else if (c == dict[p].split) {
			if (dict[p].split == '\0') {
				match = true;
				p = -1;
			} else if (dict[p].eq != -1 && dict[dict[p].eq].split == '\0') {
				match = true;
				*result = dict[dict[p].eq].value;
				p = dict[p].eq;
			} else if (dict[p].value != 0) {
				match = true;
				*result = dict[p].value;
				p = dict[p].eq;
			} else {
				p = dict[p].eq;
			}

			break;
		} else {
			p = dict[p].gt;
		}
	}

	*context = p;

	return (match) ? HUBBUB_OK :
			(p == -1) ? HUBBUB_INVALID : HUBBUB_NEEDDATA;
}

/**
 * Step-wise search for an entity in the dictionary
 *
 * \param c        Character to look for
 * \param result   Pointer to location for result
 * \param context  Pointer to location for search context
 * \return HUBBUB_OK if key found,
 *         HUBBUB_NEEDDATA if more steps are required
 *         HUBBUB_INVALID if nothing matches
 *
 * The value pointed to by ::context should be -1 for the first call.
 * Thereafter, pass in the same value as returned by the previous call.
 * The context is opaque to the caller and should not be inspected.
 *
 * The location pointed to by ::result will be set to U+FFFD unless a match
 * is found.
 */
hubbub_error hubbub_entities_search_step(uint8_t c, uint32_t *result,
		int32_t *context)
{
	if (result == NULL)
		return HUBBUB_BADPARM;

        *result = 0xFFFD;
        
	return hubbub_entity_tree_search_step(c, result, context);
}
