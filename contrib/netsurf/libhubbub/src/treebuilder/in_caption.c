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
 * Handle tokens in "in caption" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
hubbub_error handle_in_caption(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	bool handled = false;

	switch (token->type) {
	case HUBBUB_TOKEN_START_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == CAPTION || type == COL || type == COLGROUP ||
				type == TBODY || type == TD || type == TFOOT ||
				type == TH || type == THEAD || type == TR) {
			/** \todo parse error */
			err = HUBBUB_REPROCESS;
		} else {
			/* Process as if "in body" */
			err = handle_in_body(treebuilder, token);
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == CAPTION) {
			handled = true;
		} else if (type == TABLE) {
			/** \todo parse error if type == TABLE */
			err = HUBBUB_REPROCESS;
		} else if (type == BODY || type == COL || type == COLGROUP ||
				type == HTML || type == TBODY || type == TD ||
				type == TFOOT || type == TH ||
				type == THEAD || type == TR) {
			/** \todo parse error */
		} else {
			/* Process as if "in body" */
			err = handle_in_body(treebuilder, token);
		}
	}
		break;
	case HUBBUB_TOKEN_CHARACTER:
	case HUBBUB_TOKEN_COMMENT:
	case HUBBUB_TOKEN_DOCTYPE:
	case HUBBUB_TOKEN_EOF:
		/* Process as if "in body" */
		err = handle_in_body(treebuilder, token);

		break;
	}

	if (handled || err == HUBBUB_REPROCESS) {
		hubbub_ns ns;
		element_type otype = UNKNOWN;
		void *node;

		/** \todo fragment case */

		close_implied_end_tags(treebuilder, UNKNOWN);

		while (otype != CAPTION) {
			/** \todo parse error */

			element_stack_pop(treebuilder, &ns, &otype,	&node);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);
		}

		clear_active_formatting_list_to_marker(treebuilder);

		treebuilder->context.mode = IN_TABLE;
	}

	return err;
}

