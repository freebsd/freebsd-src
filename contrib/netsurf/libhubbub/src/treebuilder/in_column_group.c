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
 * Handle tokens in "in column group" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
hubbub_error handle_in_column_group(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	bool handled = false;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		err = process_characters_expect_whitespace(treebuilder,
				token, true);
		break;
	case HUBBUB_TOKEN_COMMENT:
		err = process_comment_append(treebuilder, token,
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);
		break;
	case HUBBUB_TOKEN_DOCTYPE:
		/** \todo parse error */
		break;
	case HUBBUB_TOKEN_START_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == HTML) {
			/* Process as if "in body" */
			err = handle_in_body(treebuilder, token);
		} else if (type == COL) {
			err = insert_element(treebuilder, &token->data.tag, 
					false);

			/** \todo ack sc flag */
		} else {
			err = HUBBUB_REPROCESS;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == COLGROUP) {
			/** \todo fragment case */
			handled = true;
		} else if (type == COL) {
			/** \todo parse error */
		} else {
			err = HUBBUB_REPROCESS;
		}
	}
		break;
	case HUBBUB_TOKEN_EOF:
		/** \todo fragment case */
		err = HUBBUB_REPROCESS;
		break;
	}

	if (handled || err == HUBBUB_REPROCESS) {
		hubbub_ns ns;
		element_type otype;
		void *node;

		/* Pop the current node (which will be a colgroup) */
		element_stack_pop(treebuilder, &ns, &otype, &node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);

		treebuilder->context.mode = IN_TABLE;
	}

	return err;
}

