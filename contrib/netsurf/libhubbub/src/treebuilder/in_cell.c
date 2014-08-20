/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 Andrew Sidwell <takkaria@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include "treebuilder/modes.h"
#include "treebuilder/internal.h"
#include "treebuilder/treebuilder.h"
#include "utils/utils.h"


/**
 * Clear the stack back to a table row context.
 *
 * \param treebuilder	The treebuilder instance
 */
static inline void close_cell(hubbub_treebuilder *treebuilder)
{
	hubbub_ns ns;
	element_type otype = UNKNOWN;
	void *node;

	element_type type;

	if (element_in_scope(treebuilder, TD, true)) {
		type = TD;
	} else {
		type = TH;
	}

	/* Act as if an end tag token of type `type` has been seen */

	close_implied_end_tags(treebuilder, UNKNOWN);
	/** \todo parse error */

	while (otype != type) {
		element_stack_pop(treebuilder, &ns, &otype, &node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);
	}

	clear_active_formatting_list_to_marker(treebuilder);
	treebuilder->context.mode = IN_ROW;

	return;
}


/**
 * Handle tokens in "in cell" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
hubbub_error handle_in_cell(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;

	switch (token->type) {
	case HUBBUB_TOKEN_START_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == CAPTION || type == COL ||
				type == COLGROUP || type == TBODY || 
				type == TD || type == TFOOT || type == TH || 
				type == THEAD || type == TR) {
			/** \todo fragment case */
			close_cell(treebuilder);
			err = HUBBUB_REPROCESS;
		} else {
			err = handle_in_body(treebuilder, token);
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == TH || type == TD) {
			if (element_in_scope(treebuilder, type, true)) {
				hubbub_ns ns;
				element_type otype = UNKNOWN;
				void *node;

				close_implied_end_tags(treebuilder, UNKNOWN);
				/** \todo parse error */

				while (otype != type) {
					element_stack_pop(treebuilder,
							&ns, &otype, &node);

					treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						node);
				}

				clear_active_formatting_list_to_marker(
						treebuilder);

				treebuilder->context.mode = IN_ROW;
			} else {
				/** \todo parse error */
			}
		} else if (type == BODY || type == CAPTION || type == COL ||
				type == COLGROUP || type == HTML) {
			/** \todo parse error */
		} else if (type == TABLE  || type == TBODY || type == TFOOT ||
				type == THEAD || type == TR) {
			if (element_in_scope(treebuilder, type, true)) {
				close_cell(treebuilder);
				err = HUBBUB_REPROCESS;
			} else {
				/** \todo parse error */
			}
		} else {
			err = handle_in_body(treebuilder, token);
		}
	}
		break;
	case HUBBUB_TOKEN_CHARACTER:
	case HUBBUB_TOKEN_COMMENT:
	case HUBBUB_TOKEN_DOCTYPE:
	case HUBBUB_TOKEN_EOF:
		err = handle_in_body(treebuilder, token);
		break;
	}

	return err;
}

