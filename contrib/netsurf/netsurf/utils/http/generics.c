/*
 * Copyright 2010 John-Mark Bell <jmb@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include "utils/http/generics.h"
#include "utils/http/primitives.h"

/**
 * Destructor for an item list
 *
 * \param list  List to destroy
 */
void http___item_list_destroy(http__item *list)
{
	while (list != NULL) {
		http__item *victim = list;

		list = victim->next;

		victim->free(victim);
	}
}

/**
 * Parse a list of items
 *
 * \param input       Pointer to current input byte. Updated on exit.
 * \param itemparser  Pointer to function to parse list items
 * \param first       Pointer to first item, or NULL.
 * \param parameters  Pointer to location to receive on-heap parameter list.
 * \return NSERROR_OK on success,
 * 	   NSERROR_NOMEM on memory exhaustion,
 * 	   NSERROR_NOT_FOUND if no items could be parsed
 *
 * The returned list is owned by the caller
 *
 * \note Ownership of the \a first item is passed to this function.
 */
nserror http___item_list_parse(const char **input, 
		http__itemparser itemparser, http__item *first, 
		http__item **items)
{
	const char *pos = *input;
	const char separator = *pos;
	http__item *item;
	http__item *list = first;
	nserror error = NSERROR_OK;

	/* 1*( <separator> <item> ) */

	while (*pos == separator) {
		pos++;

		http__skip_LWS(&pos);

		error = itemparser(&pos, &item);
		if (error == NSERROR_OK) {
			if (list != NULL)
				item->next = list;

			list = item;

			http__skip_LWS(&pos);
		} else if (error != NSERROR_NOT_FOUND) {
			/* Permit <separator> LWS <separator> */
			break;
		}
	}

	if (error != NSERROR_OK && error != NSERROR_NOT_FOUND) {
		http__item_list_destroy(list);
	} else if (list == NULL) {
		error = NSERROR_NOT_FOUND;
	} else {
		error = NSERROR_OK;
		*items = list;
		*input = pos;
	}

	return error;
}


